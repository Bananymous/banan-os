#include <kernel/ACPI/BatterySystem.h>
#include <kernel/FS/DevFS//FileSystem.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::ACPI
{

	static BAN::UniqPtr<BatterySystem> s_instance;

	class BatteryInfoInode final : public TmpInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<BatteryInfoInode>> create_new(
			AML::Namespace& acpi_namespace,
			const AML::Scope& battery_path,
			BAN::StringView method,
			size_t index,
			mode_t mode, uid_t uid, gid_t gid)
		{
			auto inode_info = create_inode_info(mode | Mode::IFREG, uid, gid);
			auto ino = TRY(DevFileSystem::get().allocate_inode(inode_info));

			auto battery_path_copy = TRY(battery_path.copy());
			auto method_copy = TRY(AML::NameString::from_string(method));

			auto* inode_ptr = new BatteryInfoInode(acpi_namespace, BAN::move(battery_path_copy), BAN::move(method_copy), index, ino, inode_info);
			if (inode_ptr == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			return BAN::RefPtr<BatteryInfoInode>::adopt(inode_ptr);
		}

	protected:
		BAN::ErrorOr<size_t> read_impl(off_t offset, BAN::ByteSpan buffer) override
		{
			if (offset < 0)
				return BAN::Error::from_errno(EINVAL);

			if (SystemTimer::get().ms_since_boot() > m_last_read_ms + 1000)
			{
				auto [method_path, method_ref] = TRY(m_acpi_namespace.find_named_object(m_battery_path, m_method_name));
				if (method_ref == nullptr)
					return BAN::Error::from_errno(EFAULT);

				auto result = TRY(AML::method_call(method_path, method_ref->node, {}));
				if (result.type != AML::Node::Type::Package || result.as.package->num_elements < m_result_index)
					return BAN::Error::from_errno(EFAULT);

				auto& target_elem = result.as.package->elements[m_result_index];
				if (!target_elem.resolved || !target_elem.value.node)
					return BAN::Error::from_errno(EFAULT);

				auto target_conv = AML::convert_node(TRY(target_elem.value.node->copy()), AML::ConvInteger, sizeof(uint64_t));
				if (target_conv.is_error())
					return BAN::Error::from_errno(EFAULT);

				m_last_read_ms = SystemTimer::get().ms_since_boot();
				m_last_value = target_conv.value().as.integer.value;
			}

			auto target_str = TRY(BAN::String::formatted("{}", m_last_value));

			if (static_cast<size_t>(offset) >= target_str.size())
				return 0;

			const size_t ncopy = BAN::Math::min(buffer.size(), target_str.size() - offset);
			memcpy(buffer.data(), target_str.data() + offset, ncopy);
			return ncopy;
		}

		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override		{ return BAN::Error::from_errno(EINVAL); }
		BAN::ErrorOr<void> truncate_impl(size_t) override						{ return BAN::Error::from_errno(EINVAL); }

		bool can_read_impl() const override { return true; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }

	private:
		BatteryInfoInode(AML::Namespace& acpi_namespace, AML::Scope&& battery_path, AML::NameString&& method, size_t index, ino_t ino, const TmpInodeInfo& info)
			: TmpInode(DevFileSystem::get(), ino, info)
			, m_acpi_namespace(acpi_namespace)
			, m_battery_path(BAN::move(battery_path))
			, m_method_name(BAN::move(method))
			, m_result_index(index)
		{ }

	private:
		AML::Namespace& m_acpi_namespace;
		AML::Scope m_battery_path;
		AML::NameString m_method_name;
		size_t m_result_index;

		uint64_t m_last_read_ms = 0;
		uint64_t m_last_value = 0;
	};

	BAN::ErrorOr<void> BatterySystem::initialize(AML::Namespace& acpi_namespace)
	{
		ASSERT(!s_instance);

		auto* battery_system = new BatterySystem(acpi_namespace);
		if (battery_system == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		s_instance = BAN::UniqPtr<BatterySystem>::adopt(battery_system);

		TRY(s_instance->initialize_impl());

		return {};
	}

	BatterySystem::BatterySystem(AML::Namespace& acpi_namespace)
		: m_acpi_namespace(acpi_namespace)
	{ }

	BAN::ErrorOr<void> BatterySystem::initialize_impl()
	{
		auto base_inode = TRY(TmpDirectoryInode::create_new(DevFileSystem::get(), 0555, 0, 0, static_cast<TmpInode&>(*DevFileSystem::get().root_inode())));
		DevFileSystem::get().add_inode("batteries", base_inode);

		auto batteries = TRY(m_acpi_namespace.find_device_with_eisa_id("PNP0C0A"_sv));
		for (const auto& battery : batteries)
		{
			auto [_0, sta_ref] = TRY(m_acpi_namespace.find_named_object(battery, TRY(AML::NameString::from_string("_STA"_sv))));
			if (sta_ref != nullptr)
			{
				auto sta_result = AML::evaluate_node(_0, sta_ref->node);
				if (sta_result.is_error() || sta_result.value().type != AML::Node::Type::Integer)
					continue;
				// "battery is present"
				if (!(sta_result.value().as.integer.value & 0x10))
					continue;
			}

			auto [_1, bif_ref] = TRY(m_acpi_namespace.find_named_object(battery, TRY(AML::NameString::from_string("_BIF"_sv))));
			if (!bif_ref || bif_ref->node.type != AML::Node::Type::Method || bif_ref->node.as.method.arg_count != 0)
			{
				dwarnln("Battery {} does not have _BIF or it is invalid", battery);
				continue;
			}

			auto [_2, bst_ref] = TRY(m_acpi_namespace.find_named_object(battery, TRY(AML::NameString::from_string("_BST"_sv))));
			if (!bst_ref || bst_ref->node.type != AML::Node::Type::Method || bst_ref->node.as.method.arg_count != 0)
			{
				dwarnln("Battery {} does not have _BST or it is invalid", battery);
				continue;
			}

			auto battery_name = BAN::StringView(reinterpret_cast<const char*>(&battery.parts.back()), 4);
			auto battery_inode = TRY(TmpDirectoryInode::create_new(DevFileSystem::get(), 0555, 0, 0, *base_inode));
			TRY(base_inode->link_inode(*battery_inode, battery_name));

			auto cap_full_inode = TRY(BatteryInfoInode::create_new(m_acpi_namespace, battery, "_BIF"_sv, 2, 0444, 0, 0));
			TRY(battery_inode->link_inode(*cap_full_inode, "capacity_full"_sv));

			auto cap_now_inode  = TRY(BatteryInfoInode::create_new(m_acpi_namespace, battery, "_BST"_sv, 2, 0444, 0, 0));
			TRY(battery_inode->link_inode(*cap_now_inode, "capacity_now"_sv));
		}

		return {};
	}

}
