#include <kernel/Debug.h>
#include <kernel/Panic.h>

extern "C"
{

	struct source_location
	{
		const char *file;
		uint32_t line;
		uint32_t column;
	};

	struct type_descriptor
	{
		uint16_t kind;
		uint16_t info;
		char name[1];
	};

	struct type_mismatch_data
	{
		source_location location;
		const type_descriptor& type;
		uint8_t alignment;
		uint8_t type_check_kind;
	};

	struct out_of_bounds_data
	{
		source_location location;
		const type_descriptor& left_type;
		const type_descriptor& right_type;
	};

	struct shift_out_of_bounds_data
	{
		source_location location;
		const type_descriptor& left_type;
		const type_descriptor& right_type;
	};

	struct pointer_overflow_data
	{
		source_location location;
	};

	struct invalid_value_data
	{
		source_location location;
		const type_descriptor& type;
	};

	struct overflow_data
	{
		source_location location;
		const type_descriptor& type;
	};

	struct vla_bound_data
	{
		source_location location;
		const type_descriptor& type;
	};

	enum builtin_check_kind : unsigned char
	{
		BCK_CTZPassedZero,
		BCK_CLZPassedZero,
	};

	struct invalid_builtin_data
	{
		source_location location;
		builtin_check_kind kind;
	};

	using value_handle = uintptr_t;

	static const char* type_check_kinds[] = {
		"load of", "store to", "reference binding to", "member access within",
		"member call on", "constructor call on", "downcast of", "downcast of",
		"upcast of", "cast to virtual base of", "_Nonnull binding to",
		"dynamic operation on"
	};

	#define HANDLER(handler, fatal, ...)															\
		void handler(__VA_ARGS__)																	\
		{																							\
			if constexpr(fatal)																		\
				Kernel::panic("{}:{} {}", data->location.file, data->location.line, __FUNCTION__);	\
			else																					\
				derrorln("{}:{} {}", data->location.file, data->location.line, __FUNCTION__);		\
		}

	static void print_location_impl(const source_location& location, const char* function)
	{
		derrorln("{}:{} {}", location.file, location.line, function);
	}
	#define print_location(location) print_location_impl(location, __FUNCTION__)

	HANDLER(__ubsan_handle_pointer_overflow, false, pointer_overflow_data* data, value_handle, value_handle)
	HANDLER(__ubsan_handle_load_invalid_value, false, invalid_value_data* data, value_handle)
	HANDLER(__ubsan_handle_out_of_bounds, false, out_of_bounds_data* data, value_handle)
	HANDLER(__ubsan_handle_shift_out_of_bounds, false, shift_out_of_bounds_data* data, value_handle, value_handle)

	HANDLER(__ubsan_handle_add_overflow, false, overflow_data* data, value_handle, value_handle)
	HANDLER(__ubsan_handle_sub_overflow, false, overflow_data* data, value_handle, value_handle)
	HANDLER(__ubsan_handle_mul_overflow, false, overflow_data* data, value_handle, value_handle)
	HANDLER(__ubsan_handle_divrem_overflow, false, overflow_data* data, value_handle, value_handle)
	HANDLER(__ubsan_handle_negate_overflow, false, overflow_data* data, value_handle)

	HANDLER(__ubsan_handle_vla_bound_not_positive, false, vla_bound_data* data, value_handle);

	HANDLER(__ubsan_handle_invalid_builtin, false, invalid_builtin_data* data);

	void __ubsan_handle_type_mismatch_v1(type_mismatch_data* data, value_handle pointer)
	{
		print_location(data->location);
		const char* kind = type_check_kinds[data->type_check_kind];
		uintptr_t alignment = (uintptr_t)1 << data->alignment;
		if (!pointer)
			derrorln("{} null pointer of type {}", kind, data->type.name);
		else if (pointer & (alignment - 1))
			derrorln("{} misaligned address {} for type {}, which requires {} byte alignment", kind, (void*)pointer, data->type.name, alignment);
		else
			derrorln("{} address {} with insufficient space for an object of type {}", kind, (void*)pointer, data->type.name);
	}

}