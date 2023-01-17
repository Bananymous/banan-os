#pragma once

#include <BAN/Errors.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename>
	class Function;
	template<typename Ret, typename... Args>
	class Function<Ret(Args...)>
	{
	public:
		Function() {}
		Function(Ret(*function)(Args...))
		{
			static_assert(sizeof(CallablePointer) <= m_size);
			new (m_storage) CallablePointer(function);
		}
		template<typename Own>
		Function(Ret(Own::*function)(Args...), Own* owner)
		{
			static_assert(sizeof(CallableMember<Own>) <= m_size);
			new (m_storage) CallableMember<Own>(function, owner);
		}
		template<typename Own>
		Function(Ret(Own::*function)(Args...) const, const Own* owner)
		{
			static_assert(sizeof(CallableMemberConst<Own>) <= m_size);
			new (m_storage) CallableMemberConst<Own>(function, owner);
		}

		Ret operator()(Args... args)
		{
			ASSERT(*this);
			return reinterpret_cast<CallableBase*>(m_storage)->call(Forward<Args>(args)...);
		}

		operator bool() const
		{
			for (size_t i = 0; i < m_size; i++)
				if (m_storage[i])
					return true;
			return false;
		}
		
	private:
		struct CallableBase
		{
			virtual ~CallableBase() {}
			virtual Ret call(Args...) = 0;
		};

		struct CallablePointer : public CallableBase
		{
			CallablePointer(Ret(*function)(Args...))
				: m_function(function)
			{ }

			virtual Ret call(Args... args) override
			{
				return m_function(Forward<Args>(args)...);
			}

		private:
			Ret(*m_function)(Args...) = nullptr;
		};

		template<typename Own>
		struct CallableMember : public CallableBase
		{
			CallableMember(Ret(Own::*function)(Args...), Own* owner)
				: m_owner(owner)
				, m_function(function)
			{ }

			virtual Ret call(Args... args) override
			{
				return (m_owner->*m_function)(Forward<Args>(args)...);
			}

		private:
			Own* m_owner = nullptr;
			Ret(Own::*m_function)(Args...) = nullptr;
		};

		template<typename Own>
		struct CallableMemberConst : public CallableBase
		{
			CallableMemberConst(Ret(Own::*function)(Args...) const, const Own* owner)
				: m_owner(owner)
				, m_function(function)
			{ }

			virtual Ret call(Args... args) override
			{
				return (m_owner->*m_function)(Forward<Args>(args)...);
			}

		private:
			const Own* m_owner = nullptr;
			Ret(Own::*m_function)(Args...) const = nullptr;
		};

	private:
		static constexpr size_t m_size = sizeof(void*) * 4;
		alignas(max_align_t) uint8_t m_storage[m_size] { 0 };
	};

}