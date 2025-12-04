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
}

void debug_print(PCSTR text) {
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}

namespace driver {
	namespace codes {
		constexpr ULONG attach =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_ANY_ACCESS);
		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_ANY_ACCESS);
		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_ANY_ACCESS);
	}

	struct Request {
		HANDLE pid;
		PVOID buffer;
		PVOID target;
		SIZE_T size;
		SIZE_T return_size;
	};

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return irp->IoStatus.Status = STATUS_SUCCESS;
	}

	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return irp->IoStatus.Status = STATUS_SUCCESS;
	}

	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);

		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
		}

		static PEPROCESS target_process = nullptr;
		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode;

		switch (control_code) {
		case codes::attach: {
			status = PsLookupProcessByProcessId(request->pid, &target_process);
			if (NT_SUCCESS(status)) {
				debug_print("[+] Process attached\n");
			}
			break;
		}
		case codes::read: {
			if (target_process != nullptr && request->target && request->buffer) {
				status = MmCopyVirtualMemory(
					target_process,
					request->target,
					PsGetCurrentProcess(),
					request->buffer,
					request->size,
					UserMode,
					&request->return_size
				);
			}
			break;
		}
		case codes::write: {
			if (target_process != nullptr && request->target && request->buffer) {
				status = MmCopyVirtualMemory(
					PsGetCurrentProcess(),
					request->buffer,
					target_process,
					request->target,
					request->size,
					UserMode,
					&request->return_size
				);
			}
			break;
		}
		default:
			break;
		}

		irp->IoStatus.Status = status;
		irp->IoStatus.Information = sizeof(Request);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return status;
	}
}

NTSTATUS driver_main(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {
	UNREFERENCED_PARAMETER(registry_path);
	UNICODE_STRING deviceName = {};
	RtlInitUnicodeString(&deviceName, L"\\Device\\airway_radio");

	PDEVICE_OBJECT device_object = nullptr;
	NTSTATUS status = IoCreateDevice(driver_object, 0, &deviceName,
		FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device_object);

	if (status != STATUS_SUCCESS) return status;

	UNICODE_STRING symbolicLink = {};
	RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\airway_radio");
	status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
	if (status != STATUS_SUCCESS) return status;

	SetFlag(device_object->Flags, DO_BUFFERED_IO);
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;
	ClearFlag(device_object->Flags, DO_DEVICE_INITIALIZING);

	debug_print("[+] Hiroyoshi Driver loaded\n");
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry() {
	UNICODE_STRING driverName = {};
	RtlInitUnicodeString(&driverName, L"\\Driver\\airway_radio");
	return IoCreateDriver(&driverName, driver_main);
}
