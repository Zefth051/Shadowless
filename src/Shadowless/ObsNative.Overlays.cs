using System.Runtime.InteropServices;

namespace Shadowless;

internal static partial class ObsNative
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct DisplayInfo
    {
        internal nint Window;
        internal uint Width, Height, Backbuffers;
        internal int Format, DepthFormat;
        internal uint Adapter;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void DrawCallback(nint data, uint width, uint height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint obs_display_create(ref DisplayInfo info, uint background);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_display_destroy(nint display);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_display_resize(nint display, uint width, uint height);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_display_add_draw_callback(nint display, DrawCallback callback, nint data);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_render_main_texture();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gs_set_viewport(int x, int y, int width, int height);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gs_ortho(float left, float right, float top, float bottom, float near, float far);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_sceneitem_set_pos(nint item, ref Vec2 position);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_sceneitem_set_alignment(nint item, uint alignment);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_sceneitem_set_bounds_alignment(nint item, uint alignment);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_sceneitem_set_order_position(nint item, int position);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_source_filter_add(nint source, nint filter);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_source_filter_remove(nint source, nint filter);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void obs_data_set_double(nint data,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name, double value);
}
