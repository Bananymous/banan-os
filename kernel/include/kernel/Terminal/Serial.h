#pragma once

#include <BAN/Errors.h>
#include <kernel/Terminal/TTY.h>

namespace Kernel
{

	class Serial
	{
	public:
		static void initialize();
		static bool has_devices();
		static void putchar_any(char);

		static void initialize_devices();

		void putchar(char);
		char getchar();

		bool is_valid() const { return m_port != 0; }

		uint16_t port() const { return m_port; }
		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

	private:
		static bool initialize_port(uint16_t, uint32_t baud);
		bool initialize_size();

	private:
		uint16_t m_port { 0 };
		uint32_t m_width { 0 };
		uint32_t m_height { 0 };
	};

	class SerialTTY final : public TTY
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<SerialTTY>> create(Serial);
		
		virtual uint32_t width() const override;
		virtual uint32_t height() const override;
		virtual void putchar(uint8_t) override;
	
		virtual void update() override;

	protected:
		virtual BAN::StringView name() const override { return m_name; }

	private:
		SerialTTY(Serial);
		bool initialize();

	private:
		BAN::String m_name;
		Serial m_serial;
	
	public:
		virtual dev_t rdev() const override { return m_rdev; }
	private:
		const dev_t m_rdev;
	};

}