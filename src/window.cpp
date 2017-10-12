#include "window.h"

#include "program.h"

#include <utility>

static constexpr char window_data_name_this_ptr[] = "*";

void Window::OnMove(const Window &/*from*/, const Window &to)
{
    SDL_SetWindowData(*to.window, window_data_name_this_ptr, (void *)&to);
}

SDL_Window *Window::WindowHandleFuncs::Create(std::string name, ivec2 size, Settings &settings)
{
    uint32_t flags = SDL_WINDOW_OPENGL;
    uint32_t context_flags = 0;

    if (settings.resizable)
        flags |= SDL_WINDOW_RESIZABLE;
    if (settings.fullscreen)
    {
        if (settings.keep_desktop_resolution_when_fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        else
            flags |= SDL_WINDOW_FULLSCREEN;
    }
    ivec2 pos;
    if (settings.make_centered)
        pos = ivec2(SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display));
    else
        pos = ivec2(SDL_WINDOWPOS_UNDEFINED_DISPLAY(settings.display));
    if (settings.gl_major || settings.gl_minor)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, settings.gl_major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, settings.gl_minor);
    }
    switch (settings.gl_profile)
    {
      case core:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        break;
      case compatibility:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        break;
      case es:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        break;
      default:
        break;
    }
    if (settings.gl_debug)
        context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    switch (settings.hardware_acceleration)
    {
      case yes:
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        break;
      case no:
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 0);
        break;
      default:
        break;
    }
    if (settings.forward_compatibility)
        context_flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    if (settings.msaa == 1) settings.msaa = 0;
    if (settings.msaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, settings.msaa);
    }
    if (settings.color_bits.r) SDL_GL_SetAttribute(SDL_GL_RED_SIZE    , settings.color_bits.r);
    if (settings.color_bits.g) SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE  , settings.color_bits.g);
    if (settings.color_bits.b) SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE   , settings.color_bits.b);
    if (settings.color_bits.a) SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE  , settings.color_bits.a);
    if (settings.depth_bits  ) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE  , settings.depth_bits  );
    if (settings.stencil_bits) SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, settings.stencil_bits);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

    return SDL_CreateWindow(name.c_str(), pos.x, pos.y, size.x, size.y, flags);
}
void Window::WindowHandleFuncs::Destroy(SDL_Window *window)
{
    SDL_DestroyWindow(window);
}
void Window::WindowHandleFuncs::Error(std::string name, ivec2 /*size*/, const Settings &settings)
{
    throw cant_create_window(name, Reflection::to_string_tree(settings));
}

SDL_GLContext Window::ContextHandleFuncs::Create(SDL_Window *window, std::string /*name*/, const Settings &/*settings*/)
{
    return SDL_GL_CreateContext(window);
}
void Window::ContextHandleFuncs::Destroy(SDL_GLContext context)
{
    SDL_GL_DeleteContext(context);
}
void Window::ContextHandleFuncs::Error(SDL_Window */*window*/, std::string name, const Settings &settings)
{
    throw cant_create_context(name, Reflection::to_string_tree(settings));
}

Window::Window(std::string name, ivec2 size, Settings settings)
{
    Create(name, size, settings);
}


void Window::Create(std::string new_name, ivec2 new_size, Settings new_settings)
{
    static bool need_sdl_init = 1;
    if (need_sdl_init)
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO))
            Program::Error("Can't initialize SDL video subsystem.");
        need_sdl_init = 0;
    }

    name = new_name;
    size = new_size;
    settings = new_settings;

    Handle_t new_window = {{name, size, settings}};
    ContextHandle_t new_context = {{*new_window, name, settings}};
    window  = new_window.move();
    context = new_context.move();

    if (settings.min_size)
        SDL_SetWindowMinimumSize(*window, settings.min_size.x, settings.min_size.y);

    switch (settings.swap_mode)
    {
      case no_vsync:
        if (SDL_GL_SetSwapInterval(0))
        {
            settings.swap_mode = vsync;
            SDL_GL_SetSwapInterval(1);
        }
        break;
      case vsync:
        if (SDL_GL_SetSwapInterval(1))
        {
            settings.swap_mode = no_vsync;
            SDL_GL_SetSwapInterval(0);
        }
        break;
      case late_swap_tearing:
        if (SDL_GL_SetSwapInterval(-1))
        {
            settings.swap_mode = vsync;
            if (SDL_GL_SetSwapInterval(1))
            {
                settings.swap_mode = no_vsync;
                SDL_GL_SetSwapInterval(0);
            }
        }
        break;
      default:
        break;
    }

    func_context.alloc();
    glfl::set_active_context(func_context);
    glfl::set_function_loader(SDL_GL_GetProcAddress);
    if (settings.gl_profile != es)
        glfl::load_gl(settings.gl_major, settings.gl_minor);
    else
        glfl::load_gles(settings.gl_major, settings.gl_minor);

    SDL_SetWindowData(*window, window_data_name_this_ptr, this);
}
void Window::Destroy()
{
    context.destroy();
    window.destroy();
}
bool Window::Exists() const
{
    return bool(window);
}

void Window::Activate() const
{
    SDL_GL_MakeCurrent(*window, *context);
    glfl::set_active_context(func_context);
}

const Window::Settings &Window::GetSettings() const
{
    return settings;
}

const SDL_Window *Window::GetHandle() const
{
    return *window;
}
SDL_GLContext Window::GetContextHandle() const
{
    return *context;
}

void Window::Swap() const
{
    SDL_GL_SwapWindow(*window);
}

SDL_Window *Window::Handle() const
{
    return *window;
}
SDL_GLContext Window::ContextHandle() const
{
    return *context;
}
const glfl::context &Window::FuncContext() const
{
    return *func_context;
}
const Window *Window::FromHandle(SDL_Window *handle)
{
    return (Window *)SDL_GetWindowData(handle, window_data_name_this_ptr);
}