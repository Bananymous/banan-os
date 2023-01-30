#pragma once

#define BAN_NON_COPYABLE(class) 					\
	private:										\
		class(const class&) = delete;				\
		class& operator=(const class&) = delete

#define BAN_NON_MOVABLE(class) 						\
	private:										\
		class(class&&) = delete;					\
		class& operator=(class&&) = delete
