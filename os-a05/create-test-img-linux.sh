# Create a 32 MB image filled with zeros
dd if=/dev/zero of=test.img bs=512 count=65536
# Format as FAT16 with a volume label
mkfs.fat -F 16 -n TESTDISK test.img
# Mount the image (requires root)
mkdir -p mnt
sudo mount -o loop test.img mnt
# Add some test files
echo "hello world" | sudo tee mnt/HELLO.TXT > /dev/null
echo "readme text" | sudo tee mnt/README.TXT > /dev/null
sudo mkdir -p mnt/SUBDIR/DEEP
echo "nested file" | sudo tee mnt/SUBDIR/NESTED.TXT > /dev/null
echo "deep file" | sudo tee mnt/SUBDIR/DEEP/DEEP.TXT > /dev/null
