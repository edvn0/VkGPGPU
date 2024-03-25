#pragma once

#include "Concepts.hpp"

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
class InterfaceSystem;
class Image;
class Mesh;
class Instance;
class Logger;
class Material;
class ComputePipeline;
class GraphicsPipeline;
class Window;
class QueueUnknownException;
class Shader;
class Texture;
class Timer;
class Swapchain;
class SceneRenderer;
class VulkanResultException;
struct AllocationProperties;
struct BufferDataImpl;
struct Colour;
struct ImageProperties;
struct ImageStorageImpl;
struct GraphicsPipelineConfiguration;
struct ComputePipelineConfiguration;

template <IsNumber T> struct Extent;

} // namespace Core
