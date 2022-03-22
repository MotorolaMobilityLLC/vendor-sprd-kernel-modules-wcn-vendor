c2hal:
usage: c2hal [-g] [-o dir] -p package (-r interface-root)+ (header-filepath)+
         -h print this message
         -o output path
            (example: ~/android/master)
         -p package
            (example: android.hardware.baz@1.0)
         -g (enable open-gl mode) 
         -r package:path root (e.g., android.hardware:hardware/interfaces)

c2hal -r android.hardware:vendor/sprd/modules/wcn/hardware/interfaces -randroid.hidl:system/libhidl/transport -p android.hardware.debug@1.0 vendor/sprd/modules/wcn/hardware/libwcn/wcn_vendor.h
hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/debug/1.0/default/ -Lc++-impl -randroid.hidl:system/libhidl/transport android.hardware.debug@1.0


songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen --help
hidl-gen: invalid option -- '-'
usage: hidl-gen [-p <root path>] -o <output path> -L <language> [-O <owner>] (-r <interface root>)+ [-R] [-v] [-d <depfile>] FQNAME...

Process FQNAME, PACKAGE(.SUBPACKAGE)*@[0-9]+.[0-9]+(::TYPE)?, to create output.

         -h: Prints this menu.
         -L <language>: The following options are available:
            check           : Parses the interface to see if valid but doesn't write any files.
            c++             : (internal) (deprecated) Generates C++ interface files for talking to HIDL interfaces.
            c++-headers     : (internal) Generates C++ headers for interface files for talking to HIDL interfaces.
            c++-sources     : (internal) Generates C++ sources for interface files for talking to HIDL interfaces.
            export-header   : Generates a header file from @export enumerations to help maintain legacy code.
            c++-impl        : Generates boilerplate implementation of a hidl interface in C++ (for convenience).
            c++-impl-headers: c++-impl but headers only.
            c++-impl-sources: c++-impl but sources only.
            c++-adapter     : Takes a x.(y+n) interface and mocks an x.y interface.
            c++-adapter-headers: c++-adapter but helper headers only.
            c++-adapter-sources: c++-adapter but helper sources only.
            c++-adapter-main: c++-adapter but the adapter binary source only.
            java            : (internal) Generates Java library for talking to HIDL interfaces in Java.
            java-constants  : (internal) Like export-header but for Java (always created by -Lmakefile if @export exists).
            vts             : (internal) Generates vts proto files for use in vtsd.
            makefile        : (removed) Used to generate makefiles for -Ljava and -Ljava-constants.
            androidbp       : (internal) Generates Soong bp files for -Lc++-headers, -Lc++-sources, -Ljava, -Ljava-constants, and -Lc++-adapter.
            androidbp-impl  : Generates boilerplate bp files for implementation created with -Lc++-impl.
            hash            : Prints hashes of interface in `current.txt` format to standard out.
            function-count  : Prints the total number of functions added by the package or interface.
            dependencies    : Prints all depended types.
         -O <owner>: The owner of the module for -Landroidbp(-impl)?.
         -o <output path>: Location to output files.
         -p <root path>: Android build root, defaults to $ANDROID_BUILD_TOP or pwd.
         -R: Do not add default package roots if not specified in -r.
         -r <package:path root>: E.g., android.hardware:hardware/interfaces.
         -v: verbose output.
         -d <depfile>: location of depfile to write to.

vendor.sprd.hardware.wcn@1.0--> fixed format

//vendor/sprd/modules/wcn/hardware/interfaces
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ c2hal -r android.hardware:hardware/interfaces -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/modules/wcn/hardware/interfaces -pvendor.sprd.hardware.wcn@1.0 vendor/sprd/modules/wcn/hardware/libwcn/wcn_vendor.h
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ 
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/wcn/1.0/default/ -Lc++-impl -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/modules/wcn/hardware/interfaces vendor.sprd.hardware.wcn@1.0

hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/wcn/1.0/default/ -Landroidbp-impl -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/modules/wcn/hardware/interfaces vendor.sprd.hardware.wcn@1.0



songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/wcn/1.0/default/ -Landroidbp-impl -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces vendor.sprd.hardware.wcn@1.0
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/wcn/1.0/default/ -Landroidbp -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces vendor.sprd.hardware.wcn@1.0
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen -o vendor/sprd/modules/wcn/hardware/interfaces/wcn/1.0/default/ -Lmakefile -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces vendor.sprd.hardware.wcn@1.0
ERROR: makefile output is not supported. Use -Landroidbp for all build file generation.
ERROR: output handler failed.


//vendor/sprd/intefaces/wcn
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ c2hal -r android.hardware:hardware/interfaces -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces -pvendor.sprd.hardware.wcn@1.0 vendor/sprd/modules/wcn/hardware/libwcn/wcn_vendor.h
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ 
songhe.wei@shandvm07:~/mywork/sprdroid10_trunk_phoenix_dev$ hidl-gen -o vendor/sprd/interfaces/wcn/1.0/default/ -Lc++-impl-sources -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces vendor.sprd.hardware.wcn@1.0
hidl-gen -o vendor/sprd/interfaces/wcn/1.0/default/ -Landroidbp-impl -randroid.hidl:system/libhidl/transport -rvendor.sprd.hardware:vendor/sprd/interfaces vendor.sprd.hardware.wcn@1.0


//hardware/interface/wcn
/home/newdisk/q_for_emu$ c2hal -r android.hardware:hardware/interfaces -randroid.hidl:system/libhidl/transport -pandroid.hardware.wcn@1.0 hardware/libhardware/include/hardware/wcn_vendor.h
/home/newdisk/q_for_emu$ hidl-gen -o hardware/interfaces/wcn/1.0/default/ -Lc++-impl-sources -randroid.hidl:system/libhidl/transport android.hardware.wcn@1.0
/home/newdisk/q_for_emu$ 


