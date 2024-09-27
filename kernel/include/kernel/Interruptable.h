#pragma once

namespace Kernel
{

	class Interruptable
	{
	public:
		void set_irq(int irq);

		virtual void handle_irq() = 0;

	protected:
		Interruptable() = default;
		virtual ~Interruptable() {}

	private:
		int m_irq { -1 };
	};

}
