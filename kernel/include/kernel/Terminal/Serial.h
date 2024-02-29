#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
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

	class SerialTTY final : public TTY, public Interruptable
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<SerialTTY>> create(Serial);

		virtual uint32_t width() const override;
		virtual uint32_t height() const override;

		virtual void clear() override { putchar_impl('\e'); putchar_impl('['); putchar_impl('2'); putchar_impl('J'); }

		virtual void update() override;

		virtual void handle_irq() override;

	protected:
		virtual BAN::StringView name() const override { return m_name; }
		virtual void putchar_impl(uint8_t) override;

	private:
		SerialTTY(Serial);
		bool initialize();

	private:
		BAN::String m_name;
		Serial m_serial;
		BAN::CircularQueue<uint8_t, 128> m_input;
		SpinLock m_input_lock;

	public:
		virtual dev_t rdev() const override { return m_rdev; }
	private:
		const dev_t m_rdev;
	};

}
