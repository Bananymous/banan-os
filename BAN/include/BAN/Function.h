#pragma once

#include <BAN/Errors.h>
#include <BAN/Move.h>
#include <BAN/PlacementNew.h>

namespace BAN
{

	template<typename>
	class Function;
	template<typename Ret, typename... Args>
	class Function<Ret(Args...)>
	{
	public:
		Function() = default;
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
		template<typename Lambda>
		Function(Lambda lambda)
		{
			static_assert(sizeof(CallableLambda<Lambda>) <= m_size);
			new (m_storage) CallableLambda<Lambda>(lambda);
		}

		~Function()
		{
			clear();
		}

		Ret operator()(Args... args) const
		{
			ASSERT(*this);
			return reinterpret_cast<const CallableBase*>(m_storage)->call(forward<Args>(args)...);
		}

		operator bool() const
		{
			for (size_t i = 0; i < m_size; i++)
				if (m_storage[i])
					return true;
			return false;
		}

		void clear()
		{
			if (*this)
				reinterpret_cast<CallableBase*>(m_storage)->~CallableBase();
			memset(m_storage, 0, m_size);
		}

		static constexpr size_t size() { return m_size; }

	private:
		struct CallableBase
		{
			virtual ~CallableBase() {}
			virtual Ret call(Args...) const = 0;
		};

		struct CallablePointer : public CallableBase
		{
			CallablePointer(Ret(*function)(Args...))
				: m_function(function)
			{ }

			virtual Ret call(Args... args) const override
			{
				return m_function(forward<Args>(args)...);
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

			virtual Ret call(Args... args) const override
			{
				return (m_owner->*m_function)(forward<Args>(args)...);
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

			virtual Ret call(Args... args) const override
			{
				return (m_owner->*m_function)(forward<Args>(args)...);
			}

		private:
			const Own* m_owner = nullptr;
			Ret(Own::*m_function)(Args...) const = nullptr;
		};

		template<typename Lambda>
		struct CallableLambda : public CallableBase
		{
			CallableLambda(Lambda lambda)
				: m_lambda(lambda)
			{ }

			virtual Ret call(Args... args) const override
			{
				return m_lambda(forward<Args>(args)...);
			}

		private:
			Lambda m_lambda;
		};

	private:
		static constexpr size_t m_size = sizeof(void*) * 8;
		alignas(CallableBase) uint8_t m_storage[m_size] { 0 };
	};

}
