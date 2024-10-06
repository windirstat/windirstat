Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public class ResourcePruner
{
    delegate bool EnumTypesFuncType(IntPtr hModule, IntPtr lpszType, IntPtr lParam);

    delegate bool EnumNamesFuncType(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, IntPtr lParam);

    delegate bool EnumLangsFuncType(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, ushort wIDLanguage,
        IntPtr lParam);

    [DllImport("kernel32.dll")]
    static extern IntPtr BeginUpdateResource(string pFileName, bool bDeleteExistingResources);

    [DllImport("kernel32.dll")]
    static extern bool EndUpdateResource(IntPtr hUpdate, bool fDiscard);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32.dll")]
    static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern bool EnumResourceTypes(IntPtr hModule, EnumTypesFuncType lpEnumFunc, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern bool EnumResourceNames(IntPtr hModule, IntPtr lpszType, EnumNamesFuncType lpEnumFunc, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern bool EnumResourceLanguages(IntPtr hModule, IntPtr lpszType, IntPtr lpszName,
        EnumLangsFuncType lpEnumFunc, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern bool UpdateResource(IntPtr hUpdate, IntPtr lpType, IntPtr lpName, ushort wLanguage, IntPtr lpData,
        uint cbData);

    static bool EnumLangsFunc(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, ushort wIDLanguage, IntPtr lParam)
    {
        if ((lpszName.ToInt64() >> 16) == 0) return true;

        string name = Marshal.PtrToStringUni(lpszName);
        if (name.Contains("RIBBON") || name.Contains("OFFICE"))
        {
            UpdateResource(lParam, lpszType, lpszName, wIDLanguage, IntPtr.Zero, 0);
        }
        return true;
    }

    static bool EnumNamesFunc(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, IntPtr lParam)
    {
        return EnumResourceLanguages(hModule, lpszType, lpszName, EnumLangsFunc, lParam);
    }

    static bool EnumTypesFunc(IntPtr hModule, IntPtr lpszType, IntPtr lParam)
    {
        return EnumResourceNames(hModule, lpszType, EnumNamesFunc, lParam);
    }

    public static void PruneSymbols(string file)
    {
        const uint LOAD_LIBRARY_AS_DATAFILE = 2;
        IntPtr hUpdate = BeginUpdateResource(file, false);
        IntPtr hModule = LoadLibraryEx(file, IntPtr.Zero, LOAD_LIBRARY_AS_DATAFILE);
        EnumResourceTypes(hModule, EnumTypesFunc, hUpdate);
        FreeLibrary(hModule);
        EndUpdateResource(hUpdate, false);
    }
}
'@

[ResourcePruner]::PruneSymbols($args)