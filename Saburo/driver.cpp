#include <ntifs.h>

extern "C" {
    NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName,
        PDRIVER_INITIALIZE InitializationFunction);
    NTKERNELAPI NTSTATUS MmCopyVirtualMemory(
        PEPROCESS SourceProcess,
        PVOID SourceAddress,
        PEPROCESS TargetProcess,
        PVOID TargetAddress,
        SIZE_T BufferSize,
        KPROCESSOR_MODE PreviousMode,
        PSIZE_T ReturnSize
    );
    // Minimal declaration for ZwProtectVirtualMemory
    NTSYSAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        PSIZE_T RegionSize,
        ULONG NewAccessProtection,
        PULONG OldAccessProtection
    );
}

#define DEVICE_NAME   L"\\Device\\airway_radio"
#define SYMLINK_NAME  L"\\DosDevices\\airway_radio"
#define MAX_RW_SIZE   (1u << 20) // 1 MiB safety cap

namespace driver {
    namespace codes {
        constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_ANY_ACCESS);
        constexpr ULONG read   = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_ANY_ACCESS);
        constexpr ULONG write  = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_ANY_ACCESS);
    }

    struct Request {
        HANDLE pid;          // Process ID to attach
        PVOID  buffer;       // User buffer (METHOD_BUFFERED -> SystemBuffer)
        PVOID  target;       // Target address in remote/local process
        SIZE_T size;         // Size to copy
        SIZE_T return_size;  // Bytes actually transferred
    };

    static PEPROCESS g_target = nullptr;

    __forceinline void log(const char* msg) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "%s", msg));
    }

    // IRP_MJ_CREATE / IRP_MJ_CLOSE
    static NTSTATUS on_create_close(PDEVICE_OBJECT /*dev*/, PIRP irp) {
        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    // IRP_MJ_DEVICE_CONTROL
    static NTSTATUS on_device_control(PDEVICE_OBJECT /*dev*/, PIRP irp) {
        NTSTATUS status = STATUS_INVALID_PARAMETER;
        SIZE_T transferred = 0;

        PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation(irp);
        if (!stk || stk->Parameters.DeviceIoControl.InputBufferLength < sizeof(Request)) {
            irp->IoStatus.Status = status;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return status;
        }

        auto req = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);
        const ULONG code = stk->Parameters.DeviceIoControl.IoControlCode;

        __try {
            switch (code) {
            case codes::attach: {
                // Re-attach safely
                if (g_target) {
                    ObDereferenceObject(g_target);
                    g_target = nullptr;
                }
                status = PsLookupProcessByProcessId(req->pid, &g_target);
                if (NT_SUCCESS(status)) log("[airway_radio] attached to process\n");
                break;
            }

            case codes::read: {
                if (!g_target || !req->buffer || !req->target || !req->size || req->size > MAX_RW_SIZE) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                status = MmCopyVirtualMemory(
                    g_target,                // source process
                    req->target,             // source address
                    PsGetCurrentProcess(),   // destination process (us)
                    req->buffer,             // destination address
                    req->size,
                    KernelMode,              // perform in kernel context
                    &transferred
                );
                break;
            }

            case codes::write: {
                if (!g_target || !req->buffer || !req->target || !req->size || req->size > MAX_RW_SIZE) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                // Capture requestor process before attaching
                PEPROCESS requestor = PsGetCurrentProcess();

                KAPC_STATE apc;
                KeStackAttachProcess(g_target, &apc);
                do {
                    // Page-align the protection window around target region
                    const ULONG_PTR targetAddr = reinterpret_cast<ULONG_PTR>(req->target);
                    PVOID protectBase = reinterpret_cast<PVOID>(targetAddr & ~(static_cast<ULONG_PTR>(PAGE_SIZE) - 1));
                    const ULONG_PTR endAddr = targetAddr + static_cast<ULONG_PTR>(req->size);
                    const ULONG_PTR endAligned = (endAddr + (PAGE_SIZE - 1)) & ~(static_cast<ULONG_PTR>(PAGE_SIZE) - 1);
                    SIZE_T protectSize = static_cast<SIZE_T>(endAligned - reinterpret_cast<ULONG_PTR>(protectBase));

                    ULONG oldProt = 0;
                    NTSTATUS ps = ZwProtectVirtualMemory(NtCurrentProcess(), &protectBase, &protectSize, PAGE_EXECUTE_READWRITE, &oldProt);
                    if (!NT_SUCCESS(ps)) {
                        status = ps;
                        break;
                    }

                    status = MmCopyVirtualMemory(
                        requestor,            // source process (original caller)
                        req->buffer,          // source address
                        g_target,             // destination process
                        req->target,          // destination address
                        req->size,
                        KernelMode,           // perform in kernel context
                        &transferred
                    );

                    // Restore original protection regardless of status
                    ZwProtectVirtualMemory(NtCurrentProcess(), &protectBase, &protectSize, oldProt, &oldProt);
                } while (false);
                KeUnstackDetachProcess(&apc);
                break;
            }

            default:
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
        }

        if (NT_SUCCESS(status)) {
            req->return_size = transferred;
            irp->IoStatus.Information = sizeof(Request);
        } else {
            irp->IoStatus.Information = 0;
        }
        irp->IoStatus.Status = status;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    // Mapper-safe driver entry point (no unload)
    static NTSTATUS driver_main(PDRIVER_OBJECT drv, PUNICODE_STRING /*reg*/) {

        UNICODE_STRING devName, symName;
        PDEVICE_OBJECT device = nullptr;

        RtlInitUnicodeString(&devName, DEVICE_NAME);
        NTSTATUS status = IoCreateDevice(
            drv,
            0,
            &devName,
            FILE_DEVICE_UNKNOWN,
            FILE_DEVICE_SECURE_OPEN,
            FALSE,
            &device
        );
        if (!NT_SUCCESS(status)) return status;

        SetFlag(device->Flags, DO_BUFFERED_IO);

        RtlInitUnicodeString(&symName, SYMLINK_NAME);
        status = IoCreateSymbolicLink(&symName, &devName);
        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(device);
            return status;
        }

        drv->MajorFunction[IRP_MJ_CREATE] = on_create_close;
        drv->MajorFunction[IRP_MJ_CLOSE] = on_create_close;
        drv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = on_device_control;

        ClearFlag(device->Flags, DO_DEVICE_INITIALIZING);
        log("[airway_radio] mapper-safe IOCTL driver initialized\n");
        return STATUS_SUCCESS;
    }
}

// Headless mapper entry — no registry usage
extern "C" NTSTATUS DriverEntry() {
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"\\Driver\\airway_radio");
    return IoCreateDriver(&name, driver::driver_main);
}
