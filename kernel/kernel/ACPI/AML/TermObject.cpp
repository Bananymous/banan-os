#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/PackageLength.h>
#include <kernel/ACPI/AML/TermObject.h>

namespace Kernel::ACPI
{

	// NameSpaceModifierObj

	bool AML::NameSpaceModifierObj::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::AliasOp:
			case AML::Byte::NameOp:
			case AML::Byte::ScopeOp:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::NameSpaceModifierObj> AML::NameSpaceModifierObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::NameOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Name");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(name, AML::NameString, span);
				AML_TRY_PARSE(object, AML::DataRefObject, span);

				return AML::NameSpaceModifierObj {
					.modifier = AML::NameSpaceModifierObj::Name {
						.name = name.release_value(),
						.object = object.release_value()
					}
				};
			}
			case AML::Byte::ScopeOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Scope");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE_PACKAGE(scope_span);
				AML_TRY_PARSE(name, AML::NameString, scope_span);
				AML_TRY_PARSE(term_list, AML::TermList, scope_span);

				return AML::NameSpaceModifierObj {
					.modifier = AML::NameSpaceModifierObj::Scope {
						.name = name.release_value(),
						.term_list = term_list.release_value()
					}
				};
			}
			GEN_PARSE_CASE_TODO(AliasOp)
			default:
				ASSERT_NOT_REACHED();
		}
	}


	// NamedObj

	bool AML::NamedObj::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			if (span.size() < 2)
				return false;
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtCreateFieldOp:
				case AML::Byte::ExtOpRegionOp:
				case AML::Byte::ExtPowerResOp:
				case AML::Byte::ExtProcessorOp:
				case AML::Byte::ExtThermalZoneOp:
				case AML::Byte::ExtBankFieldOp:
				case AML::Byte::ExtDataRegionOp:
				case AML::Byte::ExtMutexOp:
				case AML::Byte::ExtEventOp:
				case AML::Byte::ExtFieldOp:
				case AML::Byte::ExtDeviceOp:
				case AML::Byte::ExtIndexFieldOp:
					return true;
				default:
					return false;
			}
		}
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::ExternalOp:
			case AML::Byte::CreateDWordFieldOp:
			case AML::Byte::CreateWordFieldOp:
			case AML::Byte::CreateByteFieldOp:
			case AML::Byte::CreateBitFieldOp:
			case AML::Byte::CreateQWordFieldOp:
			case AML::Byte::MethodOp:
				return true;
			default:
				return false;
		}
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	BAN::Optional<AML::NamedObj> AML::NamedObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtOpRegionOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("OpRegion");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE(name_string, AML::NameString, span);

					auto region_space = static_cast<AML::NamedObj::OpRegion::RegionSpace>(span[0]);
					span = span.slice(1);

					AML_DEBUG_PRINT("RegionSpace");
					{
						AML_DEBUG_INDENT_SCOPE();
						AML_DEBUG_PRINT("0x{2H}", static_cast<uint8_t>(region_space));
					}

					AML_TRY_PARSE(region_offset, AML::TermArg, span);
					AML_TRY_PARSE(region_length, AML::TermArg, span);

					return AML::NamedObj {
						.object = AML::NamedObj::OpRegion {
							.name = name_string.release_value(),
							.region_space = region_space,
							.region_offset = region_offset.release_value(),
							.region_length = region_length.release_value()
						}
					};
				}
				case AML::Byte::ExtFieldOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Field");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE_PACKAGE(field_span);
					AML_TRY_PARSE(name_string, AML::NameString, field_span);
					//AML_DEBUG_TODO("FieldFlags");
					//AML_DEBUG_TODO("FieldList");

					return AML::NamedObj {
						.object = AML::NamedObj::Field {
							.name = name_string.release_value()
						}
					};
				}
				case AML::Byte::ExtDeviceOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Device");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE_PACKAGE(device_span);
					AML_TRY_PARSE(name_string, AML::NameString, device_span);
					AML_TRY_PARSE(term_list, AML::TermList, device_span);

					return AML::NamedObj {
						.object = AML::NamedObj::Device {
							.name = name_string.release_value(),
							.term_list = term_list.release_value()
						}
					};
				}
				case AML::Byte::ExtProcessorOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Processor");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE_PACKAGE(processor_span);
					AML_TRY_PARSE(name_string, AML::NameString, processor_span);

					auto processor_id = processor_span[0];
					processor_span = processor_span.slice(1);
					AML_DEBUG_PRINT("ProcessorId");
					{
						AML_DEBUG_INDENT_SCOPE();
						AML_DEBUG_PRINT("0x{2H}", processor_id);
					}

					uint32_t p_blk_address = processor_span[0] | (processor_span[1] << 8) | (processor_span[2] << 16) | (processor_span[3] << 24);
					processor_span = processor_span.slice(4);
					AML_DEBUG_PRINT("PBlkAddress");
					{
						AML_DEBUG_INDENT_SCOPE();
						AML_DEBUG_PRINT("0x{8H}", p_blk_address);
					}

					auto p_blk_length = processor_span[0];
					processor_span = processor_span.slice(1);
					AML_DEBUG_PRINT("PBlkLength");
					{
						AML_DEBUG_INDENT_SCOPE();
						AML_DEBUG_PRINT("0x{2H}", p_blk_length);
					}

					AML_TRY_PARSE(term_list, AML::TermList, processor_span);

					return AML::NamedObj {
						.object = AML::NamedObj::Processor {
							.name = name_string.release_value(),
							.processor_id = processor_id,
							.p_blk_address = p_blk_address,
							.p_blk_length = p_blk_length,
							.term_list = term_list.release_value()
						}
					};
				}
				case AML::Byte::ExtMutexOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Mutex");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE(name_string, AML::NameString, span);

					if (span.size() == 0)
						return {};
					auto sync_level = span[0];
					span = span.slice(1);

					return AML::NamedObj {
						.object = AML::NamedObj::Mutex {
							.name = name_string.release_value(),
							.sync_level = sync_level
						}
					};
				}
				GEN_PARSE_CASE_TODO(ExtCreateFieldOp)
				GEN_PARSE_CASE_TODO(ExtPowerResOp)
				GEN_PARSE_CASE_TODO(ExtThermalZoneOp)
				GEN_PARSE_CASE_TODO(ExtBankFieldOp)
				GEN_PARSE_CASE_TODO(ExtDataRegionOp)
				GEN_PARSE_CASE_TODO(ExtEventOp)
				GEN_PARSE_CASE_TODO(ExtIndexFieldOp)
				default:
					ASSERT_NOT_REACHED();
			}
		}

		switch (static_cast<AML::Byte>(span[0]))
		{
#define GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(NAME)				\
			case AML::Byte::Create##NAME##FieldOp:				\
			{													\
				span = span.slice(1);							\
																\
				AML_DEBUG_PRINT("Create{}Field", #NAME);		\
				AML_DEBUG_INDENT_SCOPE();						\
																\
				AML_TRY_PARSE(buffer, AML::TermArg, span);		\
				AML_TRY_PARSE(index, AML::TermArg, span);		\
				AML_TRY_PARSE(name, AML::NameString, span);		\
																\
				return AML::NamedObj {							\
					.object = AML::NamedObj::CreateSizedField {	\
						.type = CreateSizedField::Type::NAME,	\
						.buffer = buffer.release_value(),		\
						.index = index.release_value(),			\
						.name = name.release_value()			\
					}											\
				};												\
			}
			GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(Bit)
			GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(Byte)
			GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(Word)
			GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(DWord)
			GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD(QWord)
#undef GEN_NAMED_OBJ_CASE_CREATE_SIZED_FIELD

			case AML::Byte::MethodOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Method");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE_PACKAGE(method_span);
				AML_TRY_PARSE(name_string, AML::NameString, method_span);

				if (method_span.size() == 0)
					return {};
				auto method_flags = method_span[0];
				method_span = method_span.slice(1);

				uint8_t argument_count	= method_flags & 0x07;
				bool serialized			= method_flags & 0x08;
				uint8_t sync_level		= method_flags >> 4;

				AML_DEBUG_PRINT("ArgumentCount: {}", argument_count);
				AML_DEBUG_PRINT("Serialized: {}", serialized);
				AML_DEBUG_PRINT("SyncLevel: {}", sync_level);

				AML_TRY_PARSE(term_list, AML::TermList, method_span);

				return AML::NamedObj {
					.object = AML::NamedObj::Method {
						.name = name_string.release_value(),
						.argument_count = argument_count,
						.serialized = serialized,
						.sync_level = sync_level,
						.term_list = term_list.release_value()
					}
				};
			}
			GEN_PARSE_CASE_TODO(ExternalOp)
			default:
				ASSERT_NOT_REACHED();
		}
	}
#pragma GCC diagnostic pop


	// Object

	bool AML::Object::can_parse(BAN::ConstByteSpan span)
	{
		if (AML::NameSpaceModifierObj::can_parse(span))
			return true;
		if (AML::NamedObj::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::Object> AML::Object::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(AML::NameSpaceModifierObj);
		AML_TRY_PARSE_IF_CAN(AML::NamedObj);
		ASSERT_NOT_REACHED();
	}


	// StatementOpcode

	bool AML::StatementOpcode::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			if (span.size() < 2)
				return false;
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtFatalOp:
				case AML::Byte::ExtReleaseOp:
				case AML::Byte::ExtResetOp:
				case AML::Byte::ExtSignalOp:
				case AML::Byte::ExtSleepOp:
				case AML::Byte::ExtStallOp:
					return true;
				default:
					return false;
			}
		}
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::BreakOp:
			case AML::Byte::BreakPointOp:
			case AML::Byte::ContinueOp:
			case AML::Byte::IfOp:
			case AML::Byte::NoopOp:
			case AML::Byte::NotifyOp:
			case AML::Byte::ReturnOp:
			case AML::Byte::WhileOp:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::StatementOpcode> AML::StatementOpcode::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtReleaseOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Release");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE(mutex, AML::SuperName, span);

					return AML::StatementOpcode {
						.opcode = AML::StatementOpcode::Release {
							.mutex = mutex.release_value()
						}
					};
				}

				GEN_PARSE_CASE_TODO(ExtFatalOp)
				GEN_PARSE_CASE_TODO(ExtResetOp)
				GEN_PARSE_CASE_TODO(ExtSignalOp)
				GEN_PARSE_CASE_TODO(ExtSleepOp)
				GEN_PARSE_CASE_TODO(ExtStallOp)
				default:
					ASSERT_NOT_REACHED();
			}
		}

		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::IfOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("If");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE_PACKAGE(if_span);
				AML_TRY_PARSE(predicate, AML::TermArg, if_span);
				AML_TRY_PARSE(true_list, AML::TermList, if_span);

				TermList false_list;
				if (if_span.size() > 0 && static_cast<AML::Byte>(if_span[0]) == AML::Byte::ElseOp)
				{
					if_span = if_span.slice(1);
					AML_TRY_PARSE(opt_false_list, AML::TermList, if_span);
					false_list = opt_false_list.release_value();
				}

				return AML::StatementOpcode {
					.opcode = AML::StatementOpcode::IfElse {
						.predicate = predicate.release_value(),
						.true_list = true_list.release_value(),
						.false_list = BAN::move(false_list)
					}
				};
			}
			case AML::Byte::NotifyOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Notify");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(object, AML::SuperName, span);
				AML_TRY_PARSE(value, AML::TermArg, span);

				return AML::StatementOpcode {
					.opcode = AML::StatementOpcode::Notify {
						.object = object.release_value(),
						.value = value.release_value()
					}
				};
			}
			case AML::Byte::ReturnOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Return");
				AML_DEBUG_INDENT_SCOPE();

				AML::StatementOpcode::Return result;
				if (AML::DataRefObject::can_parse(span))
				{
					auto opt_arg = AML::DataRefObject::parse(span);
					if (!opt_arg.has_value())
						return {};
					result.arg = opt_arg.release_value();
				}
				return result;
			}

			GEN_PARSE_CASE_TODO(BreakOp)
			GEN_PARSE_CASE_TODO(BreakPointOp)
			GEN_PARSE_CASE_TODO(ContinueOp)
			GEN_PARSE_CASE_TODO(NoopOp)
			GEN_PARSE_CASE_TODO(WhileOp)
			default:
				ASSERT_NOT_REACHED();
		}

		ASSERT_NOT_REACHED();
	}


	// MethodInvocation

	bool AML::MethodInvocation::can_parse(BAN::ConstByteSpan span)
	{
		if (AML::NameString::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::MethodInvocation> AML::MethodInvocation::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		AML_TRY_PARSE(name, AML::NameString, span);

		//AML_DEBUG_TODO("Can't parse args, since number of args is not known.");
		return {};

		BAN::Vector<TermArg> term_args;
		while (span.size() > 0 && AML::TermArg::can_parse(span))
		{
			auto term_arg = AML::TermArg::parse(span);
			if (!term_arg.has_value())
				return {};
			MUST(term_args.push_back(term_arg.release_value()));
		}

		return AML::MethodInvocation {
			.name = name.release_value(),
			.term_args = BAN::move(term_args)
		};
	}


	// ExpressionOpcode

	bool AML::ExpressionOpcode::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;

		if (AML::MethodInvocation::can_parse(span))
			return true;

		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			if (span.size() < 2)
				return false;
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtCondRefOfOp:
				case AML::Byte::ExtLoadTableOp:
				case AML::Byte::ExtLoadOp:
				case AML::Byte::ExtAcquireOp:
				case AML::Byte::ExtWaitOp:
				case AML::Byte::ExtFromBCDOp:
				case AML::Byte::ExtToBCDOp:
				case AML::Byte::ExtTimerOp:
					return true;
				default:
					return false;
			}
		}

		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::PackageOp:
			case AML::Byte::VarPackageOp:
			case AML::Byte::BufferOp:
			case AML::Byte::StoreOp:
			case AML::Byte::RefOfOp:
			case AML::Byte::AddOp:
			case AML::Byte::ConcatOp:
			case AML::Byte::SubtractOp:
			case AML::Byte::IncrementOp:
			case AML::Byte::DecrementOp:
			case AML::Byte::MultiplyOp:
			case AML::Byte::DivideOp:
			case AML::Byte::ShiftLeftOp:
			case AML::Byte::ShiftRightOp:
			case AML::Byte::AndOp:
			case AML::Byte::NAndOp:
			case AML::Byte::OrOp:
			case AML::Byte::NOrOp:
			case AML::Byte::XOrOp:
			case AML::Byte::NotOp:
			case AML::Byte::FindSetLeftBitOp:
			case AML::Byte::FindSetRightBitOp:
			case AML::Byte::DerefOfOp:
			case AML::Byte::ConcatResOp:
			case AML::Byte::ModOp:
			case AML::Byte::SizeOfOp:
			case AML::Byte::IndexOp:
			case AML::Byte::MatchOp:
			case AML::Byte::ObjectTypeOp:
			case AML::Byte::LAndOp:
			case AML::Byte::LOrOp:
			case AML::Byte::LNotOp:
			case AML::Byte::LEqualOp:
			case AML::Byte::LGreaterOp:
			case AML::Byte::LLessOp:
			case AML::Byte::ToBufferOp:
			case AML::Byte::ToDecimalStringOp:
			case AML::Byte::ToHexStringOp:
			case AML::Byte::ToIntegerOp:
			case AML::Byte::ToStringOp:
			case AML::Byte::CopyObjectOp:
			case AML::Byte::MidOp:
				return true;
			default:
				return false;
		}
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	BAN::Optional<AML::ExpressionOpcode> AML::ExpressionOpcode::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		AML_TRY_PARSE_IF_CAN(AML::Buffer);
		AML_TRY_PARSE_IF_CAN(AML::Package);
		AML_TRY_PARSE_IF_CAN(AML::VarPackage);
		AML_TRY_PARSE_IF_CAN(AML::MethodInvocation);

		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtAcquireOp:
				{
					span = span.slice(2);

					AML_DEBUG_PRINT("Acquire");
					AML_DEBUG_INDENT_SCOPE();

					AML_TRY_PARSE(mutex, AML::SuperName, span);

					if (span.size() < 2)
						return {};
					uint16_t timeout = span[0] | (span[1] << 8);
					span = span.slice(2);

					return AML::ExpressionOpcode {
						.opcode = AML::ExpressionOpcode::Acquire {
							.mutex = mutex.release_value(),
							.timeout = timeout
						}
					};
				}
				GEN_PARSE_CASE_TODO(ExtCondRefOfOp)
				GEN_PARSE_CASE_TODO(ExtLoadTableOp)
				GEN_PARSE_CASE_TODO(ExtLoadOp)
				GEN_PARSE_CASE_TODO(ExtWaitOp)
				GEN_PARSE_CASE_TODO(ExtFromBCDOp)
				GEN_PARSE_CASE_TODO(ExtToBCDOp)
				GEN_PARSE_CASE_TODO(ExtTimerOp)
				default:
					ASSERT_NOT_REACHED();
			}
		}

		switch (static_cast<AML::Byte>(span[0]))
		{
#define GEN_EXPRESSION_OPCODE_CASE_UNARY_OP(NAME)						\
			case AML::Byte::NAME##Op:									\
			{															\
				span = span.slice(1);									\
																		\
				AML_DEBUG_PRINT(#NAME);									\
				AML_DEBUG_INDENT_SCOPE();								\
																		\
				AML_TRY_PARSE(object, AML::SuperName, span);			\
																		\
				return AML::ExpressionOpcode {							\
					.opcode = AML::ExpressionOpcode::UnaryOp {			\
						.type = UnaryOp::Type::NAME,					\
						.object = object.release_value()				\
					}													\
				};														\
			}
			GEN_EXPRESSION_OPCODE_CASE_UNARY_OP(Decrement)
			GEN_EXPRESSION_OPCODE_CASE_UNARY_OP(Increment)
			GEN_EXPRESSION_OPCODE_CASE_UNARY_OP(RefOf)
			GEN_EXPRESSION_OPCODE_CASE_UNARY_OP(SizeOf)
#undef GEN_EXPRESSION_OPCODE_CASE_UNARY_OP

#define GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(NAME)						\
			case AML::Byte::NAME##Op:									\
			{															\
				span = span.slice(1);									\
																		\
				AML_DEBUG_PRINT(#NAME);									\
				AML_DEBUG_INDENT_SCOPE();								\
																		\
				AML_TRY_PARSE(source1, AML::TermArg, span);				\
				AML_TRY_PARSE(source2, AML::TermArg, span);				\
				AML_TRY_PARSE(target, AML::SuperName, span);			\
																		\
				return AML::ExpressionOpcode {							\
					.opcode = AML::ExpressionOpcode::BinaryOp {			\
						.type = BinaryOp::Type::NAME,					\
						.source1 = MUST(BAN::UniqPtr<TermArg>::create(	\
							source1.release_value())					\
						),												\
						.source2 = MUST(BAN::UniqPtr<TermArg>::create(	\
							source2.release_value())					\
						),												\
						.target = target.release_value()				\
					}													\
				};														\
			}
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(Add)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(And)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(Multiply)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(NAnd)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(NOr)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(Or)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(Subtract)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(XOr)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(ShiftLeft)
			GEN_EXPRESSION_OPCODE_CASE_BINARY_OP(ShiftRight)
#undef GEN_EXPRESSION_OPCODE_CASE_BINARY_OP

#define GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(NAME)				\
			case AML::Byte::L##NAME##Op:								\
			{															\
				span = span.slice(1);									\
																		\
				AML_DEBUG_PRINT("L{}", #NAME);							\
				AML_DEBUG_INDENT_SCOPE();								\
																		\
				AML_TRY_PARSE(operand1, AML::TermArg, span);			\
				AML_TRY_PARSE(operand2, AML::TermArg, span);			\
																		\
				return AML::ExpressionOpcode {							\
					.opcode = AML::ExpressionOpcode::LogicalBinaryOp {	\
						.type = LogicalBinaryOp::Type::NAME,			\
						.operand1 = MUST(BAN::UniqPtr<TermArg>::create(	\
							operand1.release_value())					\
						),												\
						.operand2 = MUST(BAN::UniqPtr<TermArg>::create(	\
							operand2.release_value())					\
						)												\
					}													\
				};														\
			}
			GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(And)
			GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(Or)
			GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(Equal)
			GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(Greater)
			GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP(Less)
#undef GEN_EXPRESSION_OPCODE_CASE_LOGICAL_BINARY_OP

#define GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND(NAME)					\
			case AML::Byte::NAME##Op:									\
			{															\
				span = span.slice(1);									\
																		\
				AML_DEBUG_PRINT(#NAME);									\
				AML_DEBUG_INDENT_SCOPE();								\
																		\
				AML_TRY_PARSE(operand, AML::TermArg, span);				\
				AML_TRY_PARSE(target, AML::SuperName, span);			\
																		\
				return AML::ExpressionOpcode {							\
					.opcode = AML::ExpressionOpcode::NAME {				\
						.operand = MUST(BAN::UniqPtr<TermArg>::create(	\
							operand.release_value())					\
						),												\
						.target = target.release_value()				\
					}													\
				};														\
			}
			GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND(ToBuffer)
			GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND(ToDecimalString)
			GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND(ToHexString)
			GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND(ToInteger)
#undef GEN_EXPRESSION_OPCODE_CASE_TARGET_OPERAND

			case AML::Byte::StoreOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Store");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(source, AML::TermArg, span);
				AML_TRY_PARSE(target, AML::SuperName, span);

				return AML::ExpressionOpcode {
					.opcode = AML::ExpressionOpcode::Store {
						.source = MUST(BAN::UniqPtr<TermArg>::create(
							source.release_value())
						),
						.target = target.release_value()
					}
				};
			}
			GEN_PARSE_CASE_TODO(BufferOp)
			GEN_PARSE_CASE_TODO(ConcatOp)
			GEN_PARSE_CASE_TODO(DivideOp)
			GEN_PARSE_CASE_TODO(NotOp)
			GEN_PARSE_CASE_TODO(FindSetLeftBitOp)
			GEN_PARSE_CASE_TODO(FindSetRightBitOp)
			GEN_PARSE_CASE_TODO(DerefOfOp)
			GEN_PARSE_CASE_TODO(ConcatResOp)
			GEN_PARSE_CASE_TODO(ModOp)
			GEN_PARSE_CASE_TODO(IndexOp)
			GEN_PARSE_CASE_TODO(MatchOp)
			GEN_PARSE_CASE_TODO(ObjectTypeOp)
			GEN_PARSE_CASE_TODO(LNotOp)
			GEN_PARSE_CASE_TODO(ToStringOp)
			GEN_PARSE_CASE_TODO(CopyObjectOp)
			GEN_PARSE_CASE_TODO(MidOp)
			default:
				ASSERT_NOT_REACHED();
		}
	}
#pragma GCC diagnostic pop


	// TermObj

	bool AML::TermObj::can_parse(BAN::ConstByteSpan span)
	{
		if (AML::Object::can_parse(span))
			return true;
		if (AML::StatementOpcode::can_parse(span))
			return true;
		if (AML::ExpressionOpcode::can_parse(span))
			return true;
		return false;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	BAN::Optional<AML::TermObj> AML::TermObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(AML::Object);
		AML_TRY_PARSE_IF_CAN(AML::StatementOpcode);
		AML_TRY_PARSE_IF_CAN(AML::ExpressionOpcode);
		ASSERT_NOT_REACHED();
	}
#pragma GCC diagnostic pop


	// TermList

	bool AML::TermList::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() == 0)
			return true;
		return TermObj::can_parse(span);
	}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	BAN::Optional<AML::TermList> AML::TermList::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		TermList term_list;
		while (span.size() > 0)
		{
			if (!TermObj::can_parse(span))
			{
				AML_DEBUG_CANNOT_PARSE("AML::TermObj", span);
				return term_list;
			}

			auto object = TermObj::parse(span);
			if (!object.has_value())
				return term_list;

			MUST(term_list.terms.push_back(object.release_value()));
		}

		return term_list;
	}
#pragma GCC diagnostic pop


	// TermArg

	bool AML::TermArg::can_parse(BAN::ConstByteSpan span)
	{
		if (AML::ExpressionOpcode::can_parse(span))
			return true;
		if (AML::DataObject::can_parse(span))
			return true;
		if (AML::ArgObj::can_parse(span))
			return true;
		if (AML::LocalObj::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::TermArg> AML::TermArg::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(AML::ExpressionOpcode);
		AML_TRY_PARSE_IF_CAN(AML::DataObject);
		AML_TRY_PARSE_IF_CAN(AML::ArgObj);
		AML_TRY_PARSE_IF_CAN(AML::LocalObj);
		ASSERT_NOT_REACHED();
	}


	// ReferenceTypeOpcode

	bool AML::ReferenceTypeOpcode::can_parse(BAN::ConstByteSpan span)
	{
		if (MethodInvocation::can_parse(span))
			return true;
		if (span.size() < 1)
			return false;
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::RefOfOp:
			case AML::Byte::DerefOfOp:
			case AML::Byte::IndexOp:
				return true;
			default:
				return false;
		}
	}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	BAN::Optional<AML::ReferenceTypeOpcode> AML::ReferenceTypeOpcode::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		if (MethodInvocation::can_parse(span))
		{
			auto method = MethodInvocation::parse(span);
			if (!method.has_value())
				return {};
			return AML::ReferenceTypeOpcode {
				.opcode = AML::ReferenceTypeOpcode::UserTermObj {
					.method = method.release_value()
				}
			};
		}

		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::RefOfOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("RefOf");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(target, AML::SuperName, span);

				return AML::ReferenceTypeOpcode {
					.opcode = AML::ReferenceTypeOpcode::RefOf {
						.target = target.release_value()
					}
				};
			}
			case AML::Byte::DerefOfOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("DerefOf");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(source, AML::TermArg, span);

				return AML::ReferenceTypeOpcode {
					.opcode = AML::ReferenceTypeOpcode::DerefOf {
						.source = source.release_value()
					}
				};
			}
			case AML::Byte::IndexOp:
			{
				span = span.slice(1);

				AML_DEBUG_PRINT("Index");
				AML_DEBUG_INDENT_SCOPE();

				AML_TRY_PARSE(source, AML::TermArg, span);
				AML_TRY_PARSE(index, AML::TermArg, span);
				AML_TRY_PARSE(destination, AML::SuperName, span);

				return AML::ReferenceTypeOpcode {
					.opcode = AML::ReferenceTypeOpcode::Index {
						.source = source.release_value(),
						.index = index.release_value(),
						.destination = destination.release_value()
					}
				};
			}
			default:
				ASSERT_NOT_REACHED();
		}
	}
#pragma GCC diagnostic pop

}
