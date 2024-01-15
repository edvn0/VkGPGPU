#pragma once

#include <concepts>

namespace Core {

class Allocator;
class BaseException;
class Buffer;
class CommandBuffer;
class CommandDispatcher;
class DebugMarker;
class DescriptorMap;
class Device;
class DynamicLibraryLoader;
class Environment;
class FileCouldNotBeOpened;
class Image;
class Instance;
class Logger;
class Pipeline;
class QueueUnknownException;
class Shader;
class Timer;
class VulkanResultException;
struct AllocationProperties;
struct BufferDataImpl;
struct Colour;
struct FrameCommandBuffer;
struct GroupSize;
struct Image;
struct ImageProperties;
struct ImageStorageImpl;
struct IndexedQueue;
struct PipelineConfiguration;
struct QueueFeatureSupport;

template <std::integral T> struct Extent;

} // namespace Core
