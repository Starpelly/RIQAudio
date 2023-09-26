using System;
using System.Runtime.InteropServices;

namespace RIQAudio
{
    public static class RIQDLL
    {
        public const string nativeLibName = "RIQAudio";

        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RiqInitAudioDevice();

        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RiqCloseAudioDevice();

        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool IsRiqReady();

        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern Sound RiqLoadSound(string filePath);
        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RiqUnloadSound(Sound sound);
        [DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RiqPlaySound(Sound sound);
    }

    [StructLayout(LayoutKind.Sequential)]
    public partial struct Sound
    {
        public AudioStream Stream;
        public uint FrameCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public partial struct AudioStream
    {
        public IntPtr Buffer;

        public IntPtr Processor;

        public uint SampleRate;

        public uint SampleSize;

        public uint Channels;
    }
}
