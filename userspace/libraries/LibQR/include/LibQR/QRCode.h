#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Errors.h>

namespace LibQR
{

	class QRCode
	{
		BAN_NON_COPYABLE(QRCode);

	public:
		static BAN::ErrorOr<QRCode> create(size_t size);
		static BAN::ErrorOr<QRCode> generate(BAN::ConstByteSpan data);
		BAN::ErrorOr<QRCode> copy() const;

		QRCode(QRCode&& other);
		QRCode& operator=(QRCode&&) = delete;

		~QRCode();

		void set(size_t x, size_t y, bool value);

		void toggle(size_t x, size_t y);

		bool get(size_t x, size_t y) const;

		size_t size() const;

	private:
		QRCode(size_t size, uint8_t* data);

		const size_t m_size;
		uint8_t* m_data;
	};

}
