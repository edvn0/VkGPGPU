#pragma once

#include <concepts>

namespace Core {

class Allocator;
class BaseException;
class Buffer;
class CommandBuffer;
class CommandDispatcher;
class DebugMarker;
class Device;
class DynamicLibraryLoader;
class Environment;
class FileCouldNotBeOpened;
class Image;
class Instance;
class Logger;
class Pipeline;
class Window;
class QueueUnknownException;
class Shader;
class Timer;
class Swapchain;
class VulkanResultException;
struct AllocationProperties;
struct BufferDataImpl;
struct Colour;
struct Image;
struct ImageProperties;
struct ImageStorageImpl;
struct PipelineConfiguration;

template <std::integral T> struct Extent;

} // namespace Core
