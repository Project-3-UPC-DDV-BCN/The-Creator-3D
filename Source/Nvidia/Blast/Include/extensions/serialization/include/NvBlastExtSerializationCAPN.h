// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2018 NVIDIA Corporation. All rights reserved.


#pragma once

#include "capnp/serialize.h"
#include "NvBlastExtInputStream.h"
#include "NvBlastExtOutputStream.h"
#include "NvBlastArray.h"
#include "NvBlastExtSerialization.h"


namespace Nv
{
namespace Blast
{

template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
class ExtSerializationCAPN
{
public:
	static TObject*	deserializeFromBuffer(const unsigned char* input, uint64_t size);
	static TObject*	deserializeFromStream(std::istream& inputStream);

	static uint64_t	serializationBufferSize(const TObject* object);

	static bool		serializeIntoBuffer(const TObject* object, unsigned char* buffer, uint64_t maxSize, uint64_t& usedSize);
	static bool		serializeIntoBuffer(const TObject *object, unsigned char*& buffer, uint64_t& size, ExtSerialization::BufferProvider* bufferProvider = nullptr, uint64_t offset = 0);
	static bool		serializeIntoStream(const TObject* object, std::ostream& outputStream);

private:
	// Specialized
	static bool		serializeIntoBuilder(TSerializationBuilder& objectBuilder, const TObject* object);
	static bool		serializeIntoMessage(capnp::MallocMessageBuilder& message, const TObject* object);
	static TObject*	deserializeFromStreamReader(capnp::InputStreamMessageReader& message);
};


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
TObject* ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::deserializeFromBuffer(const unsigned char* input, uint64_t size)
{
	kj::ArrayPtr<const unsigned char> source(input, size);

	kj::ArrayInputStream inputStream(source);

	Nv::Blast::Array<uint64_t>::type scratch(static_cast<uint32_t>(size));
	kj::ArrayPtr<capnp::word> scratchArray((capnp::word*) scratch.begin(), size);

	capnp::InputStreamMessageReader message(inputStream, capnp::ReaderOptions(), scratchArray);

	return deserializeFromStreamReader(message);
}


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
TObject* ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::deserializeFromStream(std::istream& inputStream)
{
	ExtInputStream readStream(inputStream);

	capnp::InputStreamMessageReader message(readStream);

	return deserializeFromStreamReader(message);
}


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
uint64_t ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::serializationBufferSize(const TObject* object)
{
	capnp::MallocMessageBuilder message;

	bool result = serializeIntoMessage(message, object);

	if (result == false)
	{
		return 0;
	}

	return computeSerializedSizeInWords(message) * sizeof(uint64_t);
}


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
bool ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::serializeIntoBuffer(const TObject* object, unsigned char* buffer, uint64_t maxSize, uint64_t& usedSize)
{
	capnp::MallocMessageBuilder message;

	bool result = serializeIntoMessage(message, object);

	if (result == false)
	{
		usedSize = 0;
		return false;
	}

	uint64_t messageSize = computeSerializedSizeInWords(message) * sizeof(uint64_t);

	if (maxSize < messageSize)
	{
		NVBLAST_LOG_ERROR("When attempting to serialize into an existing buffer, the provided buffer was too small.");
		usedSize = 0;
		return false;
	}

	kj::ArrayPtr<unsigned char> outputBuffer(buffer, maxSize);
	kj::ArrayOutputStream outputStream(outputBuffer);

	capnp::writeMessage(outputStream, message);

	usedSize = messageSize;
	return true;
}


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
bool ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::serializeIntoBuffer(const TObject *object, unsigned char*& buffer, uint64_t& size, ExtSerialization::BufferProvider* bufferProvider, uint64_t offset)
{
	capnp::MallocMessageBuilder message;

	bool result = serializeIntoMessage(message, object);

	if (result == false)
	{
		buffer = nullptr;
		size = 0;
		return false;
	}

	const uint64_t blockSize = computeSerializedSizeInWords(message) * sizeof(uint64_t);

	size = blockSize + offset;

	buffer = static_cast<unsigned char *>(bufferProvider != nullptr ? bufferProvider->requestBuffer(size) : NVBLAST_ALLOC(size));

	kj::ArrayPtr<unsigned char> outputBuffer(buffer + offset, blockSize);
	kj::ArrayOutputStream outputStream(outputBuffer);

	capnp::writeMessage(outputStream, message);

	return true;
}


template<typename TObject, typename TSerializationReader, typename TSerializationBuilder>
bool ExtSerializationCAPN<TObject, TSerializationReader, TSerializationBuilder>::serializeIntoStream(const TObject* object, std::ostream& outputStream)
{
	capnp::MallocMessageBuilder message;

	bool result = serializeIntoMessage(message, object);

	if (result == false)
	{
		return false;
	}

	ExtOutputStream blastOutputStream(outputStream);

	writeMessage(blastOutputStream, message);

	return true;
}

}	// namespace Blast
}	// namespace Nv
