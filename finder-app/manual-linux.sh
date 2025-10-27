#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/build_output																#Output done step 1a)
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"									#Not sure what this is meant as
fi

mkdir -p ${OUTDIR}

if [ ! -d "${OUTDIR}" ]; then															#Quit if OUTDIR doesnt exist
    echo "Directory ${OUTDIR} does not exist. Exiting."									#Done step 1b)
    exit 1
fi							

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    
    #Deep Clean the kernel build tree													
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    
    #defconfig 
	make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    
	#vmlinux
	make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
	
	#module
	#make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules						#Skip the module install
	
	#device tree
	make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
# Copy the kernel image to the output directory
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
echo "Kernel image copied to ${OUTDIR}/Image"

# Verify the image file
if [ -f "${OUTDIR}/Image" ]; then
    echo "Kernel image exists: $(ls -la ${OUTDIR}/Image)"
else
    echo "ERROR: Kernel image not found!"
    exit 1
fi

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir "rootfs"
cd "rootfs"
mkdir -p bin dev etc home init lib lib64 proc sbin sys tmp usr var								#make rootfs
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=/${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

MainPath=$(find / -name "RequiredFiles" 2>/dev/null)
TestPath=~/assignments-3-and-later-Traxics/assignments-3-and-later-Traxics/finder-app/RequiredFiles

# Determine which path to use for RequiredFiles
if [ -n "$MainPath" ] && [ -d "$MainPath" ]; then
    echo "Using MainPath: $MainPath"
    RequiredFilesPath="$MainPath"
elif [ -d "$TestPath" ]; then
    echo "Using TestPath: $TestPath"
    RequiredFilesPath="$TestPath"
else
    cd
    echo "Current directory contents:"
    ls -la
    echo "$MainPath"
    echo "$TestPath"
    exit 1
fi

# TODO: Add library dependencies to rootfs
cp ${RequiredFilesPath}/ld-linux-aarch64.so.1 "${OUTDIR}/rootfs/lib/"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
cp ${RequiredFilesPath}/libm.so.6 "${OUTDIR}/rootfs/lib64/"
cp ${RequiredFilesPath}/libresolv.so.2 "${OUTDIR}/rootfs/lib64/"
cp ${RequiredFilesPath}/libc.so.6 "${OUTDIR}/rootfs/lib64/"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3

# TODO: Clean and build the writer utility
PathTofinderapp=$(find / -name "finder-app" 2>/dev/null)
cd ${PathTofinderapp}
make clean
make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd "${OUTDIR}/rootfs/home"
sudo cp ${PathTofinderapp}/autorun-qemu.sh "${OUTDIR}/rootfs/home"
sudo cp ${PathTofinderapp}/finder-test.sh "${OUTDIR}/rootfs/home"
sudo cp ${PathTofinderapp}/writer "${OUTDIR}/rootfs/home"
sudo cp -rL ${PathTofinderapp}/conf "${OUTDIR}/rootfs/home"
sudo cp ${PathTofinderapp}/finder.sh "${OUTDIR}/rootfs/home"

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "${OUTDIR}"
gzip -f initramfs.cpio
