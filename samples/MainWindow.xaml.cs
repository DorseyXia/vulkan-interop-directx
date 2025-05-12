using System.Diagnostics;

using Silk.NET.Core.Native;
using Silk.NET.Direct3D11;
using Silk.NET.DXGI;

using Interop.Vulkan;

using static Silk.NET.Core.Native.SilkMarshal;
using System.Windows.Threading;

#if WPF
using System.IO;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

using Silk.NET.Direct3D9;

namespace Interop.WPF;
#elif WinUI
using System.Runtime.InteropServices;

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

using Windows.Storage;
using Windows.ApplicationModel;

using WinRT;

namespace Interop.WinUI3;

[ComImport, Guid("63aad0b8-7c24-40ff-85a8-640d944cc325"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
partial interface ISwapChainPanelNative
{
    [PreserveSig]
    HResult SetSwapChain(ComPtr<IDXGISwapChain1> swapchain);
}
#endif

public sealed partial class MainWindow : Window
{
    private readonly Stopwatch stopwatch = new();

    private readonly VulkanInterop vulkanInterop = new();

    private readonly D3D11 d3d11 = D3D11.GetApi(null);

    private ComPtr<ID3D11Device> d3d11device;
    private ComPtr<ID3D11DeviceContext> d3d11context;

    private Luid dxgiAdapterLuid;
    private ComPtr<IDXGIAdapter> dxgiAdapter;
    private ComPtr<IDXGIDevice3> dxgiDevice;
    private ComPtr<IDXGIFactory2> dxgiFactory;

    private ComPtr<ID3D11Texture2D> renderTargetTexture;

    private nint renderTargetSharedHandle;
#if WinUI
    private ComPtr<IDXGISwapChain1> swapchain;

    private ComPtr<ID3D11Texture2D> backbufferTexture;

    private ComPtr<ID3D11Resource> backbufferResource;
    private ComPtr<ID3D11Resource> renderTargetResource;

    private ComPtr<IDXGIKeyedMutex> renderTargetKeyedMutex;
    private KeyedMutexSyncInfo vulkanSyncInfo;
    private KeyedMutexSyncInfo copySyncInfo;
#elif WPF
    private readonly D3D9 d3d9 = D3D9.GetApi(null);

    private ComPtr<IDirect3D9Ex> d3d9context;
    private ComPtr<IDirect3DDevice9Ex> d3d9device;

    private ComPtr<IDirect3DSurface9> d3d9surface;

    private ComPtr<IDirect3DTexture9> backbufferTexture;

    private TimeSpan lastRenderTime;
#endif

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
#if WinUI
        #region Get DXGI device, adapter and factory
        dxgiDevice = d3d11device.QueryInterface<IDXGIDevice3>();

        ThrowHResult(dxgiDevice.GetAdapter(ref dxgiAdapter));

        AdapterDesc desc = default;
        ThrowHResult(dxgiAdapter.GetDesc(ref desc));

        string name = PtrToString((nint)desc.Description);
        dxgiAdapterLuid = desc.AdapterLuid;

        dxgiFactory = dxgiAdapter.GetParent<IDXGIFactory2>();
        #endregion
#elif WPF
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
#endif
    }

    private unsafe void CreateResources(uint width, uint height)
    {
        void* handle;

#if WinUI
        #region Create swapchain and get the texture
        var swapchainDescription = new SwapChainDesc1
        {
            Width = width,
            Height = height,
            Format = Format.FormatR8G8B8A8Unorm,
            SwapEffect = SwapEffect.FlipSequential,
            SampleDesc = new SampleDesc(1u, 0u),
            BufferUsage = DXGI.UsageBackBuffer,
            BufferCount = 2u,
        };

        ThrowHResult(dxgiFactory.CreateSwapChainForComposition
        (
            dxgiDevice,
            swapchainDescription,
            default(ComPtr<IDXGIOutput>),
            ref swapchain
        ));

        backbufferTexture = swapchain.GetBuffer<ID3D11Texture2D>(0u);

        renderTarget.As<ISwapChainPanelNative>().SetSwapChain(swapchain);
        #endregion

        #region Create render target texture with shared mode
        var renderTargetDescription = new Texture2DDesc
        {
            Width = width,
            Height = height,
            Format = Format.FormatR8G8B8A8Unorm,
            BindFlags = (uint)BindFlag.RenderTarget,
            MiscFlags = (uint)(ResourceMiscFlag.SharedNthandle | ResourceMiscFlag.SharedKeyedmutex),
            SampleDesc = new SampleDesc(1u, 0u),
            ArraySize = 1u,
            MipLevels = 1u
        };

        ThrowHResult(d3d11device.CreateTexture2D(renderTargetDescription, null, ref renderTargetTexture));
        #endregion

        backbufferResource = backbufferTexture.QueryInterface<ID3D11Resource>();
        renderTargetResource = renderTargetTexture.QueryInterface<ID3D11Resource>();

        #region Get keyed mutex for render target texture and setup syncing
        renderTargetKeyedMutex = renderTargetTexture.QueryInterface<IDXGIKeyedMutex>();
        vulkanSyncInfo = new KeyedMutexSyncInfo
        {
            AcquireKey = 0, // Vulkan goes first
            ReleaseKey = 1, // Release key for copy to back buffer to run
            Timeout = 10000
        };
        copySyncInfo = new KeyedMutexSyncInfo
        {
            AcquireKey = 1,
            ReleaseKey = 0, // Release key for Vulkan to run
            Timeout = 5000
        };
        #endregion

        #region Create shared handle for render target texture
        var resource = renderTargetTexture.QueryInterface<IDXGIResource1>();
        ThrowHResult(resource.CreateSharedHandle((SecurityAttributes*)null, DXGI.SharedResourceRead | DXGI.SharedResourceWrite, (char*)null, &handle));
        resource.Dispose();
        #endregion
#elif WPF
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
#endif

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
#if WinUI
        var folder = await StorageFolder.GetFolderFromPathAsync(Package.Current.InstalledPath);
        var assetsFolder = await folder.GetFolderAsync("assets");
        var helmetFile = await assetsFolder.GetFileAsync("DamagedHelmet.glb");

        modelStream = await helmetFile.OpenStreamForReadAsync();
        format = Silk.NET.Vulkan.Format.R8G8B8A8Unorm;
        handleType = Silk.NET.Vulkan.ExternalMemoryHandleTypeFlags.D3D11TextureBit;
#elif WPF
        modelStream = File.Open("assets/DamagedHelmet.glb", FileMode.Open);
        format = Silk.NET.Vulkan.Format.B8G8R8A8Unorm;
        handleType = Silk.NET.Vulkan.ExternalMemoryHandleTypeFlags.D3D11TextureKmtBit;
#endif
        vulkanInterop.Initialize(renderTargetSharedHandle, dxgiAdapterLuid, width, height, format, handleType, modelStream);

        await modelStream.DisposeAsync();

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

        vulkanInterop.Resize(renderTargetSharedHandle, width, height);
    }

    private unsafe void OnRendering(object? sender, object e)
    {
#if WinUI
        vulkanInterop.Draw(stopwatch.ElapsedMilliseconds / 1000f, vulkanSyncInfo);

        ThrowHResult(renderTargetKeyedMutex.AcquireSync(copySyncInfo.AcquireKey, copySyncInfo.Timeout));
        d3d11context.CopyResource(backbufferResource, renderTargetResource);
        ThrowHResult(renderTargetKeyedMutex.ReleaseSync(copySyncInfo.ReleaseKey));

        ThrowHResult(swapchain.Present(0u, (uint)SwapChainFlag.None));
#elif WPF
        var args = (RenderingEventArgs)e;

        if (d3dImage.IsFrontBufferAvailable && lastRenderTime != args.RenderingTime)
        {
            d3dImage.Lock();

            vulkanInterop.Draw(stopwatch.ElapsedMilliseconds / 1000f);

            d3dImage.SetBackBuffer(D3DResourceType.IDirect3DSurface9, (nint)d3d9surface.Handle);
            d3dImage.AddDirtyRect(new Int32Rect(0, 0, d3dImage.PixelWidth, d3dImage.PixelHeight));

            d3dImage.Unlock();
            
            lastRenderTime = args.RenderingTime;
            _frameCnt++;
        }
#endif
    }

    private unsafe void ReleaseResources()
    {
#if WinUI
        renderTargetKeyedMutex.Dispose();
        renderTargetResource.Dispose();
        backbufferResource.Dispose();

        swapchain.Dispose();
#elif WPF
        d3d9surface.Dispose();
#endif
        renderTargetTexture.Dispose();

        backbufferTexture.Dispose();
        _ = backbufferTexture.Detach();
    }

    private void OnWindowClosed(object sender, object e)
    {
        _timer.Stop();
        CompositionTarget.Rendering -= OnRendering;

        vulkanInterop.Clear();

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
#if WinUI
    private void OnSwitchToggled(object sender, RoutedEventArgs e)
    {
        Action action = ((ToggleSwitch)sender).IsOn ? stopwatch.Start : stopwatch.Stop;
        action();
    }
#elif WPF
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
#endif
    public MainWindow()
    {
        InitializeComponent();
#if WinUI
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(titleBarRectangle);
#elif WPF
        DataContext = vulkanInterop;
#endif
    }
}
