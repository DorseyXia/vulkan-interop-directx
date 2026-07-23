#define WPF

using System.Diagnostics;
using System.Runtime.InteropServices;
using Silk.NET.Core.Native;
using Silk.NET.Direct3D11;
using Silk.NET.DXGI;

using Interop.Vulkan;

using static Silk.NET.Core.Native.SilkMarshal;
using System.Windows.Threading;
using UnmanagedType = System.Runtime.InteropServices.UnmanagedType;
using System.Reflection.Metadata;
using System;


using System.IO;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

using Silk.NET.Direct3D9;

namespace Interop.WPF;

public sealed partial class MainWindow : Window
{
    private readonly Stopwatch stopwatch = new();

#if CSharpVulkan
    private readonly VulkanInterop vulkanInterop = new();
#else
    private IntPtr VulkanInstance;
#endif

    private readonly D3D11 d3d11 = D3D11.GetApi(null);

    private ComPtr<ID3D11Device> d3d11device;
    private ComPtr<ID3D11DeviceContext> d3d11context;

    private Luid dxgiAdapterLuid;
    private ComPtr<IDXGIAdapter> dxgiAdapter;
    private ComPtr<IDXGIDevice3> dxgiDevice;
    private ComPtr<IDXGIFactory2> dxgiFactory;

    private ComPtr<ID3D11Texture2D> renderTargetTexture;

    private nint renderTargetSharedHandle;

    private readonly D3D9 d3d9 = D3D9.GetApi(null);

    private ComPtr<IDirect3D9Ex> d3d9context;
    private ComPtr<IDirect3DDevice9Ex> d3d9device;

    private ComPtr<IDirect3DSurface9> d3d9surface;

    private ComPtr<IDirect3DTexture9> backbufferTexture;

    private TimeSpan lastRenderTime;

    private int _frameCnt = 0;
    private DispatcherTimer _timer;
    private Stopwatch _stopwatch = new();

    private unsafe void InitializeDirectX()
    {
        #region Create device and context
        ThrowHResult(d3d11.CreateDevice(
            default(ComPtr<IDXGIAdapter>),
            D3DDriverType.Hardware, 
            nint.Zero,
            (uint)CreateDeviceFlag.BgraSupport,
            null,
            0u,
            D3D11.SdkVersion,
            ref d3d11device,
            null,
            ref d3d11context));

        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine($"Direct3D11 device: 0x{(nint)d3d11device.Handle:X8}");
        Console.WriteLine($"Direct3D11 context: 0x{(nint)d3d11context.Handle:X8}");
        #endregion


        #region Create D3D9 context
        ThrowHResult(d3d9.Direct3DCreate9Ex(D3D9.SdkVersion, ref d3d9context));

        var wih = new WindowInteropHelper(this);

        var presentParameters = new Silk.NET.Direct3D9.PresentParameters
        {
            Windowed = true,
            SwapEffect = Swapeffect.Discard,
            PresentationInterval = D3D9.PresentIntervalImmediate
        };

        uint adapter = 0;
        ThrowHResult(d3d9context.GetAdapterLUID(adapter, ref dxgiAdapterLuid));
        ThrowHResult(d3d9context.CreateDeviceEx(adapter, Devtype.Hal, wih.Handle, D3D9.CreateHardwareVertexprocessing, ref presentParameters, null, ref d3d9device));

        Console.WriteLine($"Direct3D9 device: 0x{(nint)d3d9device.Handle:X8}");
        Console.WriteLine($"Direct3D9 context: 0x{(nint)d3d9context.Handle:X8}");
        #endregion
    }

    private unsafe void CreateResources(uint width, uint height)
    {
        void* handle;

        #region Create D3D9 back buffer texture and open it on the D3D11 side as the render target
        void* d3d9shared = null;
        ThrowHResult(d3d9device.CreateTexture
        (
            width,
            height,
            1u,
            D3D9.UsageRendertarget,
            Silk.NET.Direct3D9.Format.X8R8G8B8,
            Pool.Default,
            ref backbufferTexture,
            ref d3d9shared
        ));

        Console.WriteLine($"Direct3D9 texture: 0x{(nint)backbufferTexture.Handle:X8}");

        ThrowHResult(backbufferTexture.GetSurfaceLevel(0u, ref d3d9surface));

        renderTargetTexture = d3d11device.OpenSharedResource<ID3D11Texture2D>(d3d9shared);
        #endregion

        #region Get shared handle for D3D11 render target texture
        var resource = renderTargetTexture.QueryInterface<IDXGIResource>();
        ThrowHResult(resource.GetSharedHandle(&handle));
        resource.Dispose();
        #endregion

        renderTargetSharedHandle = (nint)handle;
        Console.WriteLine($"Shared Direct3D11 render target texture: 0x{renderTargetSharedHandle:X8}");
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        _timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(1000)
        };
        _timer.Tick += (s, e) =>
        {
            var fps = _frameCnt / _stopwatch.Elapsed.TotalSeconds;
            fpsTextBlock.Text = $"FPS: {fps:f1},width: {(uint)renderTarget.ActualWidth},height: {(uint)renderTarget.ActualHeight}";
            _frameCnt = 0;
            _stopwatch.Restart();
        };
        _stopwatch.Start();

        InitializeDirectX();

        uint width = (uint)renderTarget.ActualWidth;
        uint height = (uint)renderTarget.ActualHeight;

        CreateResources(width, height);

        Stream modelStream;
        Silk.NET.Vulkan.Format format;
        Silk.NET.Vulkan.ExternalMemoryHandleTypeFlags handleType;

        format = Silk.NET.Vulkan.Format.B8G8R8A8Unorm;
        handleType = Silk.NET.Vulkan.ExternalMemoryHandleTypeFlags.D3D11TextureKmtBit;

        VulkanInstance = CreateVulkanInteropInstance();
        var luid = ((ulong)(uint)dxgiAdapterLuid.High << 32) | dxgiAdapterLuid.Low;
        uint formatint = (uint)format;
        uint handleTypeInt = (uint)handleType;
        VulkanInteropInitialize(VulkanInstance, renderTargetSharedHandle, luid, width, height, formatint, handleTypeInt,
            @"D:\Repos\vulkan-interop-directx\artifacts\bin\Interop.WPF\debug\assets\DamagedHelmet.glb");


        renderTarget.SizeChanged += OnSizeChanged;

        CompositionTarget.Rendering += OnRendering;

        _timer.Start();
    }

    private void OnSizeChanged(object sender, SizeChangedEventArgs e)
    {
        uint width = (uint)e.NewSize.Width;
        uint height = (uint)e.NewSize.Height;

        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine($"Target size: width - {width}, height - {height}");

        ReleaseResources();

        CreateResources(width, height);

#if CSharpVulkan
        vulkanInterop.Resize(renderTargetSharedHandle, width, height);
#else
        VulkanInteropResize(VulkanInstance, renderTargetSharedHandle, width, height);
#endif
    }

    private unsafe void OnRendering(object? sender, object e)
    {

        var args = (RenderingEventArgs)e;

        if (d3dImage.IsFrontBufferAvailable && lastRenderTime != args.RenderingTime)
        {
            d3dImage.Lock();


            VulkanInteropDraw(  VulkanInstance, stopwatch.ElapsedMilliseconds / 1000f);

            d3dImage.SetBackBuffer(D3DResourceType.IDirect3DSurface9, (nint)d3d9surface.Handle);
            d3dImage.AddDirtyRect(new Int32Rect(0, 0, d3dImage.PixelWidth, d3dImage.PixelHeight));

            d3dImage.Unlock();
            
            lastRenderTime = args.RenderingTime;
            _frameCnt++;
        }
        }

    private unsafe void ReleaseResources()
    {

        d3d9surface.Dispose();

        renderTargetTexture.Dispose();

        backbufferTexture.Dispose();
        _ = backbufferTexture.Detach();
    }

    private void OnWindowClosed(object sender, object e)
    {
        _timer.Stop();
        CompositionTarget.Rendering -= OnRendering;

        //vulkanInterop.Clear();

        ReleaseResources();

        dxgiFactory.Dispose();
        dxgiAdapter.Dispose();
        dxgiDevice.Dispose();
        d3d11context.Dispose();
        d3d11device.Dispose();
        d3d11.Dispose();
#if WPF
        d3d9context.Dispose();
        d3d9device.Dispose();
        d3d9.Dispose();
#endif
    }
    private void OnToggleButtonChecked(object sender, RoutedEventArgs e)
    {
        stopwatch.Start();
        rotateButton.Content = "Stop";
    }

    private void OnToggleButtonUnchecked(object sender, RoutedEventArgs e)
    {
        stopwatch.Stop();
        rotateButton.Content = "Rotate";
    }
    public MainWindow()
    {
        InitializeComponent();

        //DataContext = vulkanInterop;
    }

    [DllImport("VulkanCPP.dll")]
    private static extern IntPtr CreateVulkanInteropInstance();

    [DllImport("VulkanCPP.dll")]
    private static extern IntPtr VulkanInteropInitialize(
        IntPtr instance,
        IntPtr directTextureHandle,
        UInt64 targetDeviceLuid,
        UInt32 width,
        UInt32 height,
        UInt32 format,
        UInt32 handleType,
        [MarshalAs(UnmanagedType.LPStr)] string modelFilePath);

    [DllImport("VulkanCPP.dll")]
    private static extern void VulkanInteropDraw(IntPtr instance, float time);
    [DllImport("VulkanCPP.dll")]
    private static extern void VulkanInteropResize(IntPtr instance, IntPtr sharedTexture, UInt32 w, UInt32 h);
}
