#include <BAN/ByteSpan.h>
#include <BAN/ScopeGuard.h>

#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/USB/HID/HIDDriver.h>
#include <kernel/USB/HID/Keyboard.h>
#include <kernel/USB/HID/Mouse.h>

#define DUMP_HID_REPORT 0

namespace Kernel
{

	enum HIDRequest : uint8_t
	{
		SET_PROTOCOL = 0x0B,
	};

	enum class HIDDescriptorType : uint8_t
	{
		HID      = 0x21,
		Report   = 0x22,
		Physical = 0x23,
	};

	struct HIDDescriptor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint16_t bcdHID;
		uint8_t bCountryCode;
		uint8_t bNumDescriptors;
		struct
		{
			uint8_t bDescriptorType;
			uint16_t wItemLength;
		} __attribute__((packed)) descriptors[];
	} __attribute__((packed));
	static_assert(sizeof(HIDDescriptor) == 6);


	struct GlobalState
	{
		BAN::Optional<uint16_t> usage_page;
		BAN::Optional<int32_t> logical_minimum;
		BAN::Optional<int32_t> logical_maximum_signed;
		BAN::Optional<int32_t> logical_maximum_unsigned;
		BAN::Optional<int32_t> physical_minimum;
		BAN::Optional<int32_t> physical_maximum;
		// FIXME: support units
		BAN::Optional<uint8_t> report_id;
		BAN::Optional<uint32_t> report_size;
		BAN::Optional<uint32_t> report_count;
	};

	struct LocalState
	{
		BAN::Vector<uint32_t> usage_stack;
		BAN::Optional<uint32_t> usage_minimum;
		BAN::Optional<uint32_t> usage_maximum;
		// FIXME: support all local items
	};

	using namespace USBHID;

#if DUMP_HID_REPORT
	static void dump_hid_collection(const Collection& collection, size_t indent, bool use_report_id);
#endif

	static BAN::ErrorOr<BAN::Vector<Collection>> parse_report_descriptor(BAN::ConstByteSpan report_data, bool& out_use_report_id);

	USBHIDDriver::USBHIDDriver(USBDevice& device, const USBDevice::InterfaceDescriptor& interface)
		: m_device(device)
		, m_interface(interface)
	{}

	USBHIDDriver::~USBHIDDriver()
	{
		for (auto& device_input : m_device_inputs)
			if (device_input.device)
				DevFileSystem::get().remove_device(device_input.device);
	}

	BAN::ErrorOr<void> USBHIDDriver::initialize()
	{
		auto dma_buffer = TRY(DMARegion::create(1024));

		ASSERT(static_cast<USB::InterfaceBaseClass>(m_interface.descriptor.bInterfaceClass) == USB::InterfaceBaseClass::HID);

		bool hid_descriptor_invalid = false;
		size_t hid_descriptor_index = static_cast<size_t>(-1);
		for (size_t i = 0; i < m_interface.misc_descriptors.size(); i++)
		{
			if (static_cast<HIDDescriptorType>(m_interface.misc_descriptors[i][1]) != HIDDescriptorType::HID)
				continue;
			if (m_interface.misc_descriptors[i].size() < sizeof(HIDDescriptor))
				hid_descriptor_invalid = true;
			const auto& hid_descriptor = *reinterpret_cast<const HIDDescriptor*>(m_interface.misc_descriptors[i].data());
			if (hid_descriptor.bLength != m_interface.misc_descriptors[i].size())
				hid_descriptor_invalid = true;
			if (hid_descriptor.bLength != sizeof(HIDDescriptor) + hid_descriptor.bNumDescriptors * 3)
				hid_descriptor_invalid = true;
			hid_descriptor_index = i;
			break;
		}

		if (hid_descriptor_index >= m_interface.misc_descriptors.size())
		{
			dwarnln("HID device does not contain HID descriptor");
			return BAN::Error::from_errno(EFAULT);
		}
		if (hid_descriptor_invalid)
		{
			dwarnln("HID device contains an invalid HID descriptor");
			return BAN::Error::from_errno(EFAULT);
		}

		// If this device supports boot protocol, make sure it is not used
		if (m_interface.descriptor.bInterfaceSubClass == 0x01)
		{
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Interface;
			request.bRequest      = HIDRequest::SET_PROTOCOL;
			request.wValue        = 1; // report protocol
			request.wIndex        = m_interface.descriptor.bInterfaceNumber;
			request.wLength       = 0;
			TRY(m_device.send_request(request, 0));
		}

		const auto& hid_descriptor = *reinterpret_cast<const HIDDescriptor*>(m_interface.misc_descriptors[hid_descriptor_index].data());
		dprintln_if(DEBUG_USB_HID, "HID descriptor ({} bytes)", m_interface.misc_descriptors[hid_descriptor_index].size());
		dprintln_if(DEBUG_USB_HID, "  bLength:         {}",       hid_descriptor.bLength);
		dprintln_if(DEBUG_USB_HID, "  bDescriptorType: {}",       hid_descriptor.bDescriptorType);
		dprintln_if(DEBUG_USB_HID, "  bcdHID:          {H}.{2H}", hid_descriptor.bcdHID >> 8, hid_descriptor.bcdHID & 0xFF);
		dprintln_if(DEBUG_USB_HID, "  bCountryCode:    {}",       hid_descriptor.bCountryCode);
		dprintln_if(DEBUG_USB_HID, "  bNumDescriptors: {}",       hid_descriptor.bNumDescriptors);

		uint32_t report_descriptor_index = 0;
		BAN::Vector<Collection> collections;
		for (size_t i = 0; i < hid_descriptor.bNumDescriptors; i++)
		{
			auto descriptor = hid_descriptor.descriptors[i];

			if (static_cast<HIDDescriptorType>(descriptor.bDescriptorType) != HIDDescriptorType::Report)
			{
				dprintln_if(DEBUG_USB_HID, "Skipping HID descriptor type 0x{2H}", descriptor.bDescriptorType);
				continue;
			}

			if (descriptor.wItemLength > dma_buffer->size())
			{
				dwarnln("Too big report descriptor size {} bytes ({} supported)", +descriptor.wItemLength, dma_buffer->size());
				return BAN::Error::from_errno(ENOBUFS);
			}

			{
				USBDeviceRequest request;
				request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Interface;
				request.bRequest      = USB::Request::GET_DESCRIPTOR;
				request.wValue        = (static_cast<uint16_t>(HIDDescriptorType::Report) << 8) | report_descriptor_index++;
				request.wIndex        = m_interface.descriptor.bInterfaceNumber;
				request.wLength       = descriptor.wItemLength;
				auto transferred = TRY(m_device.send_request(request, dma_buffer->paddr()));

				if (transferred < descriptor.wItemLength)
				{
					dwarnln("HID device did not respond with full report descriptor");
					return BAN::Error::from_errno(EFAULT);
				}
			}

			dprintln_if(DEBUG_USB_HID, "Parsing {} byte report descriptor", +descriptor.wItemLength);

			auto report_data = BAN::ConstByteSpan(reinterpret_cast<uint8_t*>(dma_buffer->vaddr()), descriptor.wItemLength);
			auto new_collections = TRY(parse_report_descriptor(report_data, m_uses_report_id));
			for (auto& collection : new_collections)
				TRY(collections.push_back(BAN::move(collection)));
		}

		if (collections.empty())
		{
			dwarnln("No collections specified for HID device");
			return BAN::Error::from_errno(EFAULT);
		}

		m_device_inputs = TRY(initializes_device_reports(collections));

		for (const auto& endpoint : m_interface.endpoints)
		{
			const auto& desc = endpoint.descriptor;

			if (!(desc.bEndpointAddress & 0x80))
				continue;
			if ((desc.bmAttributes & 0x03) != 0x03)
				continue;

			TRY(m_device.configure_endpoint(desc));
			m_data_buffer = TRY(DMARegion::create(desc.wMaxPacketSize & 0x07FF));

			m_data_endpoint_id = (desc.bEndpointAddress & 0x0F) * 2 + !!(desc.bEndpointAddress & 0x80);

			break;
		}

		if (m_data_endpoint_id == 0)
		{
			dwarnln("HID device does not an interrupt IN endpoints");
			return BAN::Error::from_errno(EINVAL);
		}

		m_device.send_data_buffer(m_data_endpoint_id, m_data_buffer->paddr(), m_data_buffer->size());

		return {};
	}

	static BAN::ErrorOr<void> gather_collection_inputs(const USBHID::Collection& collection, BAN::Vector<USBHID::Report>& output)
	{
		for (const auto& entry : collection.entries)
		{
			if (entry.has<USBHID::Collection>())
			{
				TRY(gather_collection_inputs(entry.get<USBHID::Collection>(), output));
				continue;
			}

			const auto& report = entry.get<USBHID::Report>();
			if (report.type != USBHID::Report::Type::Input)
				continue;

			TRY(output.push_back(report));
		}

		return {};
	}

	BAN::ErrorOr<BAN::Vector<USBHIDDriver::DeviceReport>> USBHIDDriver::initializes_device_reports(const BAN::Vector<USBHID::Collection>& collection_list)
	{
		BAN::Vector<USBHIDDriver::DeviceReport> result;
		TRY(result.reserve(collection_list.size()));

		for (size_t i = 0; i < collection_list.size(); i++)
		{
			const auto& collection = collection_list[i];

			USBHIDDriver::DeviceReport report;
			TRY(gather_collection_inputs(collection, report.inputs));

			if (collection.usage_page == 0x01)
			{
				switch (collection.usage_id)
				{
					case 0x02:
						report.device = TRY(BAN::RefPtr<USBMouse>::create());
						dprintln("Initialized an USB Mouse");
						break;
					case 0x06:
						report.device = TRY(BAN::RefPtr<USBKeyboard>::create());
						dprintln("Initialized an USB Keyboard");
						break;
					default:
						dwarnln("Unsupported generic descript page usage 0x{2H}", collection.usage_id);
						break;
				}
			}

			TRY(result.push_back(BAN::move(report)));
		}

		for (auto& report : result)
			if (report.device)
				DevFileSystem::get().add_device(report.device);
		return BAN::move(result);
	}

	void USBHIDDriver::handle_stall(uint8_t endpoint_id)
	{
		(void)endpoint_id;
		// FIXME: do something :)
	}

	void USBHIDDriver::handle_input_data(size_t byte_count, uint8_t endpoint_id)
	{
		if (m_data_endpoint_id != endpoint_id)
			return;

		auto data = BAN::ConstByteSpan(reinterpret_cast<uint8_t*>(m_data_buffer->vaddr()), byte_count);
		BAN::ScopeGuard _([&] {
			m_device.send_data_buffer(m_data_endpoint_id, m_data_buffer->paddr(), m_data_buffer->size());
		});

		if constexpr(DEBUG_USB_HID)
		{
			const auto nibble_to_hex = [](uint8_t x) -> char { return x + (x < 10 ? '0' : 'A' - 10); };

			char buffer[512];
			char* ptr = buffer;
			for (size_t i = 0; i < BAN::Math::min<size_t>((sizeof(buffer) - 1) / 3, data.size()); i++)
			{
				*ptr++ = nibble_to_hex(data[i] >> 4);
				*ptr++ = nibble_to_hex(data[i] & 0xF);
				*ptr++ = ' ';
			}
			*ptr = '\0';

			dprintln_if(DEBUG_USB_HID, "Received {} bytes from endpoint {}: {}", data.size(), endpoint_id, buffer);
		}

		const auto extract_bits =
			[&data](size_t bit_offset, size_t bit_count, bool as_unsigned) -> int64_t
			{
				if (bit_offset >= data.size() * 8)
					return 0;
				if (bit_count + bit_offset > data.size() * 8)
					bit_count = data.size() * 8 - bit_offset;

				uint32_t result = 0;
				uint32_t result_offset = 0;

				while (result_offset < bit_count)
				{
					const uint32_t byte  = bit_offset / 8;
					const uint32_t bit   = bit_offset % 8;
					const uint32_t count = BAN::Math::min<uint32_t>(bit_count - result_offset, 8 - bit);
					const uint32_t mask  = (1 << count) - 1;

					result |= static_cast<uint32_t>((data[byte] >> bit) & mask) << result_offset;

					bit_offset    += count;
					result_offset += count;
				}

				if (!as_unsigned && (result & (1u << (bit_count - 1))))
				{
					const uint32_t mask = (1u << bit_count) - 1;
					return -(static_cast<int64_t>(~result & mask) + 1);
				}

				return result;
			};

		BAN::Optional<uint8_t> report_id;
		if (m_uses_report_id)
		{
			report_id = data[0];
			data = data.slice(1);
		}

		size_t bit_offset = 0;
		for (auto& device_input : m_device_inputs)
		{
			if (device_input.device)
				device_input.device->start_report();

			for (const auto& input : device_input.inputs)
			{
				if (report_id.value_or(input.report_id) != input.report_id)
					continue;

				ASSERT(input.report_size <= 32);

				if (!device_input.device || (input.usage_id == 0 && input.usage_minimum == 0 && input.usage_maximum == 0))
				{
					bit_offset += input.report_size * input.report_count;
					continue;
				}

				for (uint32_t i = 0; i < input.report_count; i++)
				{
					const int64_t logical = extract_bits(bit_offset, input.report_size, input.logical_minimum >= 0);
					if (logical < input.logical_minimum || logical > input.logical_maximum)
					{
						bit_offset += input.report_size;
						continue;
					}

					const int64_t physical =
						(input.physical_maximum - input.physical_minimum) *
						(logical - input.logical_minimum) /
						(input.logical_maximum - input.logical_minimum) +
						input.physical_minimum;

					const uint32_t usage_base = input.usage_id ? input.usage_id : input.usage_minimum;
					if (input.flags & 0x02)
						device_input.device->handle_variable(input.usage_page, usage_base + i, physical);
					else
						device_input.device->handle_array(input.usage_page, usage_base + physical);

					bit_offset += input.report_size;
				}
			}

			if (device_input.device)
				device_input.device->stop_report();
		}
	}

	BAN::ErrorOr<BAN::Vector<Collection>> parse_report_descriptor(BAN::ConstByteSpan report_data, bool& out_use_report_id)
	{
		BAN::Vector<GlobalState> global_stack;
		GlobalState global_state;

		LocalState local_state;

		BAN::Vector<Collection> result_stack;
		BAN::Vector<Collection> collection_stack;

		bool one_has_report_id = false;
		bool all_has_report_id = true;

		const auto extract_report_item =
			[&](bool as_unsigned) -> int64_t
			{
				uint32_t value = 0;
				auto value_data = report_data.slice(1);
				switch (report_data[0] & 0x03)
				{
					case 1: value = as_unsigned ? value_data.as<const uint8_t>()  : value_data.as<const int8_t>();  break;
					case 2: value = as_unsigned ? value_data.as<const uint16_t>() : value_data.as<const int16_t>(); break;
					case 3: value = as_unsigned ? value_data.as<const uint32_t>() : value_data.as<const int32_t>(); break;
				}
				return value;
			};

		constexpr auto get_correct_sign =
			[](int64_t min, int64_t max_signed, int64_t max_unsigned) -> int64_t
			{
				if (min < 0 || max_signed >= 0)
					return max_signed;
				return max_unsigned;
			};

		const auto add_data_item =
			[&](Report::Type type, uint32_t item_data, BAN::Vector<BAN::Variant<Collection, Report>>& container) -> BAN::ErrorOr<void>
			{
				if (!global_state.report_count.has_value() || !global_state.report_size.has_value())
				{
					dwarnln("Report count and/or report size is not defined");
					return BAN::Error::from_errno(EFAULT);
				}
				if (!global_state.usage_page.has_value())
				{
					dwarnln("Usage page is not defined");
					return BAN::Error::from_errno(EFAULT);
				}
				if (!global_state.logical_minimum.has_value() || !global_state.logical_maximum_signed.has_value())
				{
					dwarnln("Logical minimum and/or logical maximum is not defined");
					return BAN::Error::from_errno(EFAULT);
				}
				if (global_state.physical_minimum.has_value() != global_state.physical_minimum.has_value())
				{
					dwarnln("Only one of physical minimum and physical maximum is defined");
					return BAN::Error::from_errno(EFAULT);
				}
				if (local_state.usage_minimum.has_value() != local_state.usage_maximum.has_value())
				{
					dwarnln("Only one of logical minimum and logical maximum is defined");
					return BAN::Error::from_errno(EFAULT);
				}

				if (global_state.report_id.has_value())
					one_has_report_id = true;
				else
					all_has_report_id = false;

				const int64_t logical_minimum = global_state.logical_minimum.value();
				const int64_t logical_maximum = get_correct_sign(
					global_state.logical_minimum.value(),
					global_state.logical_maximum_signed.value(),
					global_state.logical_maximum_unsigned.value()
				);

				int64_t physical_minimum = logical_minimum;
				int64_t physical_maximum = logical_maximum;
				if (global_state.physical_minimum.has_value() && (global_state.physical_minimum.value() || global_state.physical_maximum.value()))
				{
					physical_minimum = global_state.physical_minimum.value();
					physical_maximum = global_state.physical_maximum.value();
				}

				if (local_state.usage_stack.empty())
				{
					if (local_state.usage_minimum.has_value() && local_state.usage_maximum.has_value())
					{
						Report item;
						item.usage_page       = global_state.usage_page.value();
						item.usage_id         = 0;
						item.usage_minimum    = local_state.usage_minimum.value();
						item.usage_maximum    = local_state.usage_maximum.value();
						item.type             = type;
						item.report_id        = global_state.report_id.value_or(0);
						item.report_count     = global_state.report_count.value();
						item.report_size      = global_state.report_size.value();
						item.logical_minimum  = logical_minimum;
						item.logical_maximum  = logical_maximum;
						item.physical_minimum = physical_minimum;
						item.physical_maximum = physical_maximum;
						item.flags            = item_data;
						TRY(container.push_back(item));

						return {};
					}

					Report item;
					item.usage_page       = global_state.usage_page.value();
					item.usage_id         = 0;
					item.usage_minimum    = 0;
					item.usage_maximum    = 0;
					item.type             = type;
					item.report_id        = global_state.report_id.value_or(0);
					item.report_count     = global_state.report_count.value();
					item.report_size      = global_state.report_size.value();
					item.logical_minimum  = 0;
					item.logical_maximum  = 0;
					item.physical_minimum = 0;
					item.physical_maximum = 0;
					item.flags            = item_data;
					TRY(container.push_back(item));

					return {};
				}

				for (size_t i = 0; i < local_state.usage_stack.size(); i++)
				{
					const uint32_t usage = local_state.usage_stack[i];
					const uint32_t count = (i + 1 < local_state.usage_stack.size()) ? 1 : global_state.report_count.value() - i;

					Report item;
					item.usage_page       = (usage >> 16) ? (usage >> 16) : global_state.usage_page.value();
					item.usage_id         = usage & 0xFFFF;
					item.usage_minimum    = 0;
					item.usage_maximum    = 0;
					item.type             = type;
					item.report_id        = global_state.report_id.value_or(0);
					item.report_count     = count;
					item.report_size      = global_state.report_size.value();
					item.logical_minimum  = logical_minimum;
					item.logical_maximum  = logical_maximum;
					item.physical_minimum = physical_minimum;
					item.physical_maximum = physical_maximum;
					item.flags            = item_data;
					TRY(container.push_back(item));
				}

				return {};
			};

		while (report_data.size() > 0)
		{
			const uint8_t item_size =  report_data[0]       & 0x03;
			const uint8_t item_type = (report_data[0] >> 2) & 0x03;
			const uint8_t item_tag  = (report_data[0] >> 4) & 0x0F;

			if (item_type == 0)
			{
				switch (item_tag)
				{
					case 0b1000: // input
						if (collection_stack.empty())
						{
							dwarnln("Invalid input item outside of collection");
							return BAN::Error::from_errno(EFAULT);
						}
						TRY(add_data_item(Report::Type::Input, extract_report_item(true), collection_stack.back().entries));
						break;
					case 0b1001: // output
						if (collection_stack.empty())
						{
							dwarnln("Invalid input item outside of collection");
							return BAN::Error::from_errno(EFAULT);
						}
						TRY(add_data_item(Report::Type::Output, extract_report_item(true), collection_stack.back().entries));
						break;
					case 0b1011: // feature
						if (collection_stack.empty())
						{
							dwarnln("Invalid input item outside of collection");
							return BAN::Error::from_errno(EFAULT);
						}
						TRY(add_data_item(Report::Type::Feature, extract_report_item(true), collection_stack.back().entries));
						break;
					case 0b1010: // collection
					{
						if (local_state.usage_stack.size() != 1)
						{
							dwarnln("{} usages specified for collection", local_state.usage_stack.empty() ? "No" : "Multiple");
							return BAN::Error::from_errno(EFAULT);
						}
						uint16_t usage_page = 0;
						if (global_state.usage_page.has_value())
							usage_page = global_state.usage_page.value();
						if (local_state.usage_stack.front() >> 16)
							usage_page = local_state.usage_stack.front() >> 16;
						if (usage_page == 0)
						{
							dwarnln("Usage page not specified for a collection");
							return BAN::Error::from_errno(EFAULT);
						}

						TRY(collection_stack.emplace_back());
						collection_stack.back().usage_page = usage_page;
						collection_stack.back().usage_id   = local_state.usage_stack.front();
						break;
					}
					case 0b1100: // end collection
						if (collection_stack.empty())
						{
							dwarnln("End collection outside of collection");
							return BAN::Error::from_errno(EFAULT);
						}
						if (collection_stack.size() == 1)
						{
							TRY(result_stack.push_back(BAN::move(collection_stack.back())));
							collection_stack.pop_back();
						}
						else
						{
							TRY(collection_stack[collection_stack.size() - 2].entries.push_back(BAN::move(collection_stack.back())));
							collection_stack.pop_back();
						}
						break;
					default:
						dwarnln("Report has reserved main item tag 0b{4b}", item_tag);
						return BAN::Error::from_errno(EFAULT);
				}

				local_state = LocalState();
			}
			else if (item_type == 1)
			{
				switch (item_tag)
				{
					case 0b0000: // usage page
						global_state.usage_page = extract_report_item(true);
						break;
					case 0b0001: // logical minimum
						global_state.logical_minimum = extract_report_item(false);
						break;
					case 0b0010: // logical maximum
						global_state.logical_maximum_signed = extract_report_item(false);
						global_state.logical_maximum_unsigned = extract_report_item(true);
						break;
					case 0b0011: // physical minimum
						global_state.physical_minimum = extract_report_item(false);
						break;
					case 0b0100: // physical maximum
						global_state.physical_maximum = extract_report_item(false);
						break;
					case 0b0101: // unit exponent
						dwarnln("Report units are not supported");
						return BAN::Error::from_errno(ENOTSUP);
					case 0b0110: // unit
						dwarnln("Report units are not supported");
						return BAN::Error::from_errno(ENOTSUP);
					case 0b0111: // report size
						global_state.report_size = extract_report_item(true);
						break;
					case 0b1000: // report id
					{
						auto report_id = extract_report_item(true);
						if (report_id > 0xFF)
						{
							dwarnln("Multi-byte report id");
							return BAN::Error::from_errno(EFAULT);
						}
						global_state.report_id = report_id;
						break;
					}
					case 0b1001: // report count
						global_state.report_count = extract_report_item(true);
						break;
					case 0b1010: // push
						TRY(global_stack.push_back(global_state));
						break;
					case 0b1011: // pop
						if (global_stack.empty())
						{
							dwarnln("Report pop from empty stack");
							return BAN::Error::from_errno(EFAULT);
						}
						global_state = global_stack.back();
						global_stack.pop_back();
						break;
					default:
						dwarnln("Report has reserved global item tag 0b{4b}", item_tag);
						return BAN::Error::from_errno(EFAULT);
				}
			}
			else if (item_type == 2)
			{
				switch (item_tag)
				{
					case 0b0000: // usage
						TRY(local_state.usage_stack.emplace_back(extract_report_item(true)));
						break;
					case 0b0001: // usage minimum
						local_state.usage_minimum = extract_report_item(true);
						break;
					case 0b0010: // usage maximum
						local_state.usage_maximum = extract_report_item(true);
						break;
					case 0b0011: // designator index
					case 0b0100: // designator minimum
					case 0b0101: // designator maximum
					case 0b0111: // string index
					case 0b1000: // string minimum
					case 0b1001: // string maximum
					case 0b1010: // delimeter
						dwarnln("Unsupported local item tag 0b{4b}", item_tag);
						return BAN::Error::from_errno(ENOTSUP);
					default:
						dwarnln("Report has reserved local item tag 0b{4b}", item_tag);
						return BAN::Error::from_errno(EFAULT);
				}
			}
			else
			{
				dwarnln("Report has reserved item type 0b{2b}", item_type);
				return BAN::Error::from_errno(EFAULT);
			}

			report_data = report_data.slice(1 + item_size);
		}

		if (result_stack.empty())
		{
			dwarnln("No collection defined in report descriptor");
			return BAN::Error::from_errno(EFAULT);
		}

		if (one_has_report_id != all_has_report_id)
		{
			dwarnln("Some but not all reports have report id");
			return BAN::Error::from_errno(EFAULT);
		}

#if DUMP_HID_REPORT
		{
			SpinLockGuard _(Debug::s_debug_lock);
			for (const auto& collection : result_stack)
				dump_hid_collection(collection, 0, one_has_report_id);
		}
#endif

		out_use_report_id = one_has_report_id;
		return BAN::move(result_stack);
	}

#if DUMP_HID_REPORT
	static void print_indent(size_t indent)
	{
		for (size_t i = 0; i < indent; i++)
			Debug::putchar(' ');
	}

	static void dump_hid_report(const Report& report, size_t indent, bool use_report_id)
	{
		const char* report_type = "";
		switch (report.type)
		{
			case Report::Type::Input:   report_type = "input";   break;
			case Report::Type::Output:  report_type = "output";  break;
			case Report::Type::Feature: report_type = "feature"; break;
		}
		print_indent(indent);
		BAN::Formatter::println(Debug::putchar, "report {}", report_type);

		if (use_report_id)
		{
			print_indent(indent + 4);
			BAN::Formatter::println(Debug::putchar, "report id:  {2H}", report.report_id);
		}

		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "usage page: {2H}", report.usage_page);

		if (report.usage_id || report.usage_minimum || report.usage_maximum)
		{
			print_indent(indent + 4);
			if (report.usage_id)
				BAN::Formatter::println(Debug::putchar, "usage:      {2H}", report.usage_id);
			else
				BAN::Formatter::println(Debug::putchar, "usage:      {2H}->{2H}", report.usage_minimum, report.usage_maximum);
		}

		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "flags:      0b{8b}", report.flags);

		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "size:       {}", report.report_size);
		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "count:      {}", report.report_count);

		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "lminimum:   {}", report.logical_minimum);
		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "lmaximum:   {}", report.logical_maximum);

		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "pminimum:   {}", report.physical_minimum);
		print_indent(indent + 4);
		BAN::Formatter::println(Debug::putchar, "pmaximum:   {}", report.physical_maximum);
	}

	static void dump_hid_collection(const Collection& collection, size_t indent, bool use_report_id)
	{
		print_indent(indent);
		BAN::Formatter::println(Debug::putchar, "collection {}", collection.type);
		print_indent(indent);
		BAN::Formatter::println(Debug::putchar, "usage {H}:{H}", collection.usage_page, collection.usage_id);

		for (const auto& entry : collection.entries)
		{
			if (entry.has<Collection>())
				dump_hid_collection(entry.get<Collection>(), indent + 4, use_report_id);
			if (entry.has<Report>())
				dump_hid_report(entry.get<Report>(), indent + 4, use_report_id);
		}
	}
#endif

}
