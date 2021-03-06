#pragma once
#pragma pack(1)

#include "BlamBaseTypes.h"

struct tag_ref
{
	int tag_type;
	void *tag_pointer;
	int field_8;
	int tag_index;
};
CHECK_STRUCT_SIZE(tag_ref, 16);

struct tag_block_ref
{
	size_t size;
	void *data;
	void *definition;
};
CHECK_STRUCT_SIZE(tag_block_ref, 12);

template<typename T>
struct tag_block
{
	size_t size;
	T *data;
	void *defination;

	tag_block_ref* get_ref() {
		return reinterpret_cast<tag_block_ref*>(this);
	}
	tag_block_ref *operator&()
	{
		return get_ref();
	}

	T *operator[](size_t index)
	{
		if (index == NONE || reinterpret_cast<size_t>(this->data) == NONE)
			return nullptr;
		if (index >= this->size)
			return nullptr;
		if (LOG_CHECK(this->data))
			return &this->data[index];
		else
			return nullptr;
	}

	T *begin()
	{
		return this->data;
	}

	T *end()
	{
		if (this->data)
			return &this->data[this->size];
		else
			return nullptr;
	}

	void clear()
	{
		this->size = 0;
		this->data = reinterpret_cast<T*>(NONE);
	}
};

struct byte_ref
{
	int size;
	int stream_flags;
	int stream_offset;
	void *address;
	int definition;
};