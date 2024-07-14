#pragma once

#include <stdint.h>

namespace Kernel::XHCI
{

	struct CapabilityRegs
	{
		uint32_t caplength      : 8;
		uint32_t                : 8;
		uint32_t minor_revision : 8;
		uint32_t major_revision : 8;

		union
		{
			uint32_t hcsparams1_raw;
			struct
			{
				uint32_t max_slots        : 8;
				uint32_t max_interrupters : 11;
				uint32_t                  : 5;
				uint32_t max_ports        : 8;
			} hcsparams1;
		};

		union
		{
			uint32_t hcsparams2_raw;
			struct
			{
				uint32_t isochronous_scheduling_threshold : 4;
				uint32_t event_ring_segment_table_max     : 4;
				uint32_t                                  : 13;
				uint32_t max_scratchpad_buffers_hi        : 5;
				uint32_t scratchpad_restore               : 1;
				uint32_t max_scratchpad_buffers_lo        : 5;
			} hcsparams2;
		};

		union
		{
			uint32_t hcsparams3_raw;
			struct
			{
				uint32_t u1_device_exit_latency : 8;
				uint32_t                        : 8;
				uint32_t u2_device_exit_latency : 16;
			} hcsparams3;
		};

		union
		{
			uint32_t hccparams1_raw;
			struct
			{
				uint32_t addressing_capability64                : 1;
				uint32_t bw_negotation_capability               : 1;
				uint32_t context_size                           : 1;
				uint32_t port_power_control                     : 1;
				uint32_t port_indicators                        : 1;
				uint32_t light_hc_reset_capability              : 1;
				uint32_t latency_tolerance_messaging_capability : 1;
				uint32_t no_secondary_sid_support               : 1;
				uint32_t parse_all_event_data                   : 1;
				uint32_t stopped_short_packet_capability        : 1;
				uint32_t stopped_edtla_capability               : 1;
				uint32_t contiguous_frame_id_capability         : 1;
				uint32_t maximum_primary_stream_array_size      : 4;
				uint32_t xhci_extended_capabilities_pointer     : 16;
			} hccparams1;
		};

		uint32_t dboff;
		uint32_t rstoff;

		union
		{
			uint32_t hccparams2_raw;
			struct
			{
				uint32_t u3_entry_capability                                         : 1;
				uint32_t configure_endpoint_command_max_latency_too_large_capability : 1;
				uint32_t force_save_context_capability                               : 1;
				uint32_t compliance_transition_capability                            : 1;
				uint32_t large_esit_payload_capability                               : 1;
				uint32_t configuration_information_capability                        : 1;
				uint32_t extended_tbc_capability                                     : 1;
				uint32_t extended_tbc_trb_status_capability                          : 1;
				uint32_t get_set_extended_property_capability                        : 1;
				uint32_t virtualiaztion_based_trusted_io_capability                  : 1;
				uint32_t                                                             : 22;
			} hccparams2;
		};
	};
	static_assert(sizeof(CapabilityRegs) == 0x20);

	struct PortRegs
	{
		uint32_t portsc;
		uint32_t portmsc;
		uint32_t portli;
		uint32_t porthlpmc;
	};
	static_assert(sizeof(PortRegs) == 0x10);

	struct OperationalRegs
	{
		union
		{
			uint32_t usbcmd_raw;
			struct
			{
				uint32_t run_stop                       : 1;
				uint32_t host_controller_reset          : 1;
				uint32_t interrupter_enable             : 1;
				uint32_t host_system_error_enable       : 1;
				uint32_t                                : 3;
				uint32_t light_host_controller_reset    : 1;
				uint32_t controller_save_state          : 1;
				uint32_t controller_restore_state       : 1;
				uint32_t enable_wrap_event              : 1;
				uint32_t enable_u3_mfindex_stop         : 1;
				uint32_t                                : 1;
				uint32_t cem_enable                     : 1;
				uint32_t extended_tbc_enable            : 1;
				uint32_t extended_tbc_trb_status_enable : 1;
				uint32_t vtio_enable                    : 1;
				uint32_t                                : 15;
			} usbcmd;
		};

		uint32_t usbsts;
		uint32_t pagesize;
		uint32_t __reserved0[2];
		uint32_t dnctrl;
		uint32_t crcr_lo;
		uint32_t crcr_hi;
		uint32_t __reserved1[4];
		uint32_t dcbaap_lo;
		uint32_t dcbaap_hi;

		union
		{
			uint32_t config_raw;
			struct
			{
				uint32_t max_device_slots_enabled         : 8;
				uint32_t u3_entry_enable                  : 1;
				uint32_t configuration_information_enable : 1;
				uint32_t                                  : 22;
			} config;
		};

		uint32_t __reserved2[241];
		PortRegs ports[];
	};
	static_assert(sizeof(OperationalRegs) == 0x400);

	struct InterrupterRegs
	{
		uint32_t iman;
		uint32_t imod;
		uint32_t erstsz;
		uint32_t __reserved;
		uint64_t erstba;
		uint64_t erdp;
	};
	static_assert(sizeof(InterrupterRegs) == 0x20);

	struct RuntimeRegs
	{
		uint32_t mfindex;
		uint32_t __reserved[7];
		InterrupterRegs irs[];
	};
	static_assert(sizeof(RuntimeRegs) == 0x20);

	struct TRB
	{
		union
		{
			struct
			{
				uint64_t parameter;
				uint32_t status;
				uint16_t cycle    : 1;
				uint16_t ent      : 1;
				uint16_t          : 8;
				uint16_t trb_type : 6;
				uint16_t control;
			};

			struct
			{
				uint32_t dword0;
				uint32_t dword1;
				uint32_t dword2;
				uint32_t dword3;
			} raw;

			struct
			{
				uint64_t data_buffer_pointer       : 64;

				uint32_t trb_transfer_length       : 17;
				uint32_t td_size                   : 5;
				uint32_t interrupt_target          : 10;

				uint32_t cycle_bit                 : 1;
				uint32_t evaluate_next_trb         : 1;
				uint32_t interrupt_on_short_packet : 1;
				uint32_t no_snoop                  : 1;
				uint32_t chain_bit                 : 1;
				uint32_t interrupt_on_completion   : 1;
				uint32_t immediate_data            : 1;
				uint32_t                           : 2;
				uint32_t block_event_interrupt     : 1;
				uint32_t trb_type                  : 6;
				uint32_t                           : 16;
			} normal;

			struct
			{
				uint32_t bmRequestType           : 8;
				uint32_t bRequest                : 8;
				uint32_t wValue                  : 16;

				uint32_t wIndex                  : 16;
				uint32_t wLength                 : 16;

				uint32_t trb_transfer_length     : 17;
				uint32_t                         : 5;
				uint32_t interrupt_target        : 10;

				uint32_t cycle_bit               : 1;
				uint32_t                         : 4;
				uint32_t interrupt_on_completion : 1;
				uint32_t immediate_data          : 1;
				uint32_t                         : 3;
				uint32_t trb_type                : 6;
				uint32_t transfer_type           : 2;
				uint32_t                         : 14;
			} setup_stage;

			struct
			{
				uint64_t data_buffer_pointer       : 64;

				uint32_t trb_transfer_length       : 17;
				uint32_t td_size                   : 5;
				uint32_t interrupt_target          : 10;

				uint32_t cycle_bit                 : 1;
				uint32_t evaluate_next_trb         : 1;
				uint32_t interrupt_on_short_packet : 1;
				uint32_t no_snoop                  : 1;
				uint32_t chain_bit                 : 1;
				uint32_t interrupt_on_completion   : 1;
				uint32_t immediate_data            : 1;
				uint32_t                           : 3;
				uint32_t trb_type                  : 6;
				uint32_t direction                 : 1;
				uint32_t                           : 15;
			} data_stage;

			struct
			{
				uint32_t                         : 32;
				uint32_t                         : 32;

				uint32_t                         : 22;
				uint32_t interrupter_target      : 10;

				uint32_t cycle_bit               : 1;
				uint32_t evaluate_next_trb       : 1;
				uint32_t                         : 2;
				uint32_t chain_bit               : 1;
				uint32_t interrupt_on_completion : 1;
				uint32_t                         : 4;
				uint32_t trb_type                : 6;
				uint32_t direction               : 1;
				uint32_t                         : 15;
			} status_stage;

			struct
			{
				uint64_t trb_pointer         : 64;

				uint32_t trb_transfer_length : 24;
				uint32_t completion_code     : 8;

				uint32_t cycle_bit           : 1;
				uint32_t                     : 1;
				uint32_t event_data          : 1;
				uint32_t                     : 7;
				uint32_t trb_type            : 6;
				uint32_t endpoint_id         : 5;
				uint32_t                     : 3;
				uint32_t slot_id             : 8;
			} transfer_event;

			struct
			{
				uint64_t command_trb_pointer          : 64;
				uint32_t command_completion_parameter : 24;
				uint32_t completion_code              : 8;
				uint32_t cycle_bit                    : 1;
				uint32_t                              : 9;
				uint32_t trb_type                     : 6;
				uint32_t vf_id                        : 8;
				uint32_t slot_id                      : 8;
			} command_completion_event;

			struct
			{
				uint32_t                 : 24;
				uint32_t port_id         : 8;
				uint32_t                 : 32;
				uint32_t                 : 24;
				uint32_t completion_code : 8;
				uint32_t cycle           : 1;
				uint32_t                 : 9;
				uint32_t trb_type        : 6;
				uint32_t                 : 16;
			} port_status_chage_event;

			struct
			{
				uint32_t           : 32;
				uint32_t           : 32;
				uint32_t           : 32;
				uint32_t cycle     : 1;
				uint32_t           : 9;
				uint32_t trb_type  : 6;
				uint32_t slot_type : 5;
				uint32_t           : 11;
			} enable_slot_command;

			struct
			{
				uint32_t           : 32;
				uint32_t           : 32;
				uint32_t           : 32;
				uint32_t cycle     : 1;
				uint32_t           : 9;
				uint32_t trb_type  : 6;
				uint32_t           : 8;
				uint32_t slot_id   : 8;
			} disable_slot_command;

			struct
			{
				uint64_t input_context_pointer     : 64;
				uint32_t                           : 32;
				uint32_t cycle_bit                 : 1;
				uint32_t                           : 8;
				uint32_t block_set_address_request : 1;
				uint32_t trb_type                  : 6;
				uint32_t                           : 8;
				uint32_t slot_id                   : 8;
			} address_device_command;

			struct
			{
				uint64_t input_context_pointer : 64;
				uint32_t                       : 32;
				uint32_t cycle_bit             : 1;
				uint32_t                       : 8;
				uint32_t deconfigure           : 1;
				uint32_t trb_type              : 6;
				uint32_t                       : 8;
				uint32_t slot_id               : 8;
			} configure_endpoint_command;

			struct
			{
				uint64_t ring_segment_ponter     : 64;

				uint32_t                         : 22;
				uint32_t interrupter_target      : 10;

				uint32_t cycle_bit               : 1;
				uint32_t toggle_cycle            : 1;
				uint32_t                         : 2;
				uint32_t chain_bit               : 1;
				uint32_t interrupt_on_completion : 1;
				uint32_t                         : 4;
				uint32_t trb_type                : 6;
				uint32_t                         : 16;
			} link_trb;
		};
	};
	static_assert(sizeof(TRB) == 0x10);

	struct EventRingTableEntry
	{
		uint64_t rsba;
		uint32_t rsz;
		uint32_t __reserved;
	};
	static_assert(sizeof(EventRingTableEntry) == 0x10);

	struct ExtendedCap
	{
		uint32_t capability_id   : 8;
		uint32_t next_capability : 8;
		uint32_t                 : 16;
	};
	static_assert(sizeof(ExtendedCap) == 4);

	struct USBLegacySupportCap
	{
		uint32_t capability_id           : 8;
		uint32_t next_capability         : 8;
		uint32_t hc_bios_owned_semaphore : 1;
		uint32_t                         : 7;
		uint32_t hc_os_owned_semaphore   : 1;
		uint32_t                         : 7;
	};
	static_assert(sizeof(USBLegacySupportCap) == 4);

	struct SupportedPrococolCap
	{
		uint32_t capability_id           : 8;
		uint32_t next_capability         : 8;
		uint32_t minor_revision          : 8;
		uint32_t major_revision          : 8;
		uint32_t name_string             : 32;
		uint32_t compatible_port_offset  : 8;
		uint32_t compatible_port_count   : 8;
		uint32_t protocol_defied         : 12;
		uint32_t protocol_speed_id_count : 4;
		uint32_t protocol_slot_type      : 5;
		uint32_t                         : 27;
	};
	static_assert(sizeof(SupportedPrococolCap) == 0x10);

	struct SlotContext
	{
		uint32_t route_string         : 20;
		uint32_t speed                : 4;
		uint32_t                      : 1;
		uint32_t multi_tt             : 1;
		uint32_t hub                  : 1;
		uint32_t context_entries      : 5;

		uint16_t max_exit_latency;
		uint8_t root_hub_port_number;
		uint8_t number_of_ports;

		uint32_t parent_hub_slot_id   : 8;
		uint32_t parent_port_number   : 8;
		uint32_t tt_think_time        : 2;
		uint32_t                      : 4;
		uint32_t interrupter_target   : 10;

		uint32_t usb_device_address   : 8;
		uint32_t                      : 19;
		uint32_t slot_state           : 5;

		uint32_t                      : 32;
		uint32_t                      : 32;
		uint32_t                      : 32;
		uint32_t                      : 32;
	};
	static_assert(sizeof(SlotContext) == 0x20);

	struct EndpointContext
	{
		uint32_t endpoint_state        : 3;
		uint32_t                       : 5;
		uint32_t mult                  : 2;
		uint32_t max_primary_streams   : 5;
		uint32_t linear_stream_array   : 1;
		uint32_t interval              : 8;
		uint32_t max_esit_payload_hi   : 8;

		uint32_t                       : 1;
		uint32_t error_count           : 2;
		uint32_t endpoint_type         : 3;
		uint32_t                       : 1;
		uint32_t host_initiate_disable : 1;
		uint32_t max_burst_size        : 8;
		uint32_t max_packet_size       : 16;

		// LSB is dequeue cycle state
		uint64_t tr_dequeue_pointer;

		uint32_t average_trb_length    : 16;
		uint32_t max_esit_payload_lo   : 16;

		uint32_t                       : 32;
		uint32_t                       : 32;
		uint32_t                       : 32;
	};
	static_assert(sizeof(EndpointContext) == 0x20);

	struct InputControlContext
	{
		uint32_t drop_context_flags;
		uint32_t add_context_flags;
		uint32_t                     : 32;
		uint32_t                     : 32;
		uint32_t                     : 32;
		uint32_t                     : 32;
		uint32_t                     : 32;
		uint8_t  configuration_value;
		uint8_t  interface_number;
		uint8_t  alternate_setting;
		uint8_t                      : 8;
	};
	static_assert(sizeof(InputControlContext) == 0x20);

	enum USBSTS
	{
		HCHalted            = 1 << 0,
		HostSystemError     = 1 << 2,
		EventInterrupt      = 1 << 3,
		PortChangeDetect    = 1 << 4,
		SaveStateStatus     = 1 << 8,
		RstoreStateStatus   = 1 << 9,
		SaveRestoreError    = 1 << 10,
		ControllerNotReady  = 1 << 11,
		HostControllerError = 1 << 12,
	};

	enum CRCR : uint32_t
	{
		RingCycleState     = 1 << 0,
		CommandStop        = 1 << 1,
		CommandAbort       = 1 << 2,
		CommandRingRunning = 1 << 3,
	};

	enum IMAN : uint32_t
	{
		InterruptPending = 1 << 0,
		InterruptEnable  = 1 << 1,
	};

	enum ERDP
	{
		EventHandlerBusy = 1 << 3,
	};

	enum PORTSC : uint32_t
	{
		CCS = 1u << 0,
		PED = 1u << 1,
		OCA = 1u << 3,
		PR  = 1u << 4,
		PP  = 1u << 9,
		LWS = 1u << 16,
		CSC = 1u << 17,
		PEC = 1u << 18,
		WRC = 1u << 19,
		OCC = 1u << 20,
		PRC = 1u << 21,
		PLC = 1u << 22,
		CEC = 1u << 23,
		CAS = 1u << 24,
		WCE = 1u << 25,
		WDE = 1u << 26,
		WOE = 1u << 27,
		DR  = 1u << 30,
		WPR = 1u << 31,

		PLS_SHIFT = 5,
		PLS_MASK  = 0xF,
		PORT_SPEED_SHIFT = 10,
		PORT_SPEED_MASK  = 0xF,
		PIC_SHIFT = 14,
		PIC_MASK  = 0x3,
	};

	enum ExtendedCapabilityID : uint8_t
	{
		USBLegacySupport = 1,
		SupportedProtocol = 2,
		ExtendedPowerManagement = 3,
		IOVirtualization = 4,
		MessageInterrupt = 5,
		LocalMemory = 6,
		USBDebugCapability = 10,
		ExtendedMessageInterrupt = 17,
	};

	enum TRBType
	{
		Normal                          = 1,
		SetupStage                      = 2,
		DataStage                       = 3,
		StatusStage                     = 4,

		Link                            = 6,

		EnableSlotCommand               = 9,
		DisableSlotCommand              = 10,
		AddressDeviceCommand            = 11,
		ConfigureEndpointCommand        = 12,
		EvaluateContextCommand          = 13,
		ResetEndpointCommand            = 14,
		StopEndpointCommand             = 15,
		SetTRDequeuePointerCommand      = 16,
		ResetDeviceCommand              = 17,
		ForceEventCommand               = 18,
		NegotiateBandwidthCommand       = 19,
		SetLatencyToleranceValueCommand = 20,
		GetPortBandwidthCommand         = 21,
		ForceHeaderCommand              = 22,
		NoOpCommand                     = 23,
		GetExtendedPropertyCommand      = 24,
		SetExtendedPropertyCommand      = 25,

		TransferEvent           = 32,
		CommandCompletionEvent  = 33,
		PortStatusChangeEvent   = 34,
		BandwidthRequestEvent   = 35,
		DoorbellEvent           = 36,
		HostControllerEvent     = 37,
		DeviceNotificationEvent = 38,
		MFINDEXWrapEvent        = 39,
	};

	enum EndpointType
	{
		IsochOut     = 1,
		BulkOut      = 2,
		InterruptOut = 3,
		Control      = 4,
		IsochIn      = 5,
		BulkIn       = 6,
		InterruptIn  = 7,
	};

}
