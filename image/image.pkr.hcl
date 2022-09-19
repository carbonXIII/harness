variable "input_image_path" {
  type = string
  default = "./rootfs.tar.gz"
}

variable "project_root" {
  type = string
  default = "."
}

packer {
  required_plugins {
    arm = {
      version = "1.0.0"
      source = "github.com/cdecoux/builder-arm"
    }
  }
}

source "arm" "image" {
  file_checksum_type    = "none"
  file_target_extension = "tar"
  file_unarchive_cmd = ["bsdtar", "-xpf", "$ARCHIVE_PATH", "-C", "$MOUNTPOINT"]
  file_urls             = ["${var.input_image_path}"]
  image_build_method    = "new"
  image_chroot_env      = ["PATH=/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin"]
  image_path                   = "harness.img"
  image_size                   = "2G"
  image_type                   = "dos"
  image_partitions {
    filesystem   = "vfat"
    mountpoint   = "/boot"
    name         = "boot"
    size         = "256M"
    start_sector = "8192"
    type         = "c"
  }
  image_partitions {
    filesystem   = "ext4"
    mountpoint   = "/"
    name         = "root"
    size         = "0"
    start_sector = "532480"
    type         = "83"
  }
  qemu_binary_destination_path = "/usr/bin/qemu-arm-static"
  qemu_binary_source_path      = "/usr/bin/qemu-arm-static"
}

build {
  sources = ["source.arm.image"]

  provisioner "file" {
    source = "${var.project_root}/build/image/firmware/"
    destination = "/boot/"
  }

  provisioner "file" {
    source = "${var.project_root}/image/overlay/"
    destination = "/"
  }

  provisioner "file" {
    source = "${var.project_root}/build/image/pi_builder-prefix/src/pi_builder-build/pi/harness_server"
    destination = "/usr/bin/harness_server"
  }

  provisioner "shell" {
    inline = ["chmod 755 /usr/bin/harness_server"]
  }

  provisioner "shell-local" {
    inline = [
      "cat ${var.project_root}/image/fstab | sed s/uuid/$(od -A n -X -j 440 -N 4 harness.img | xargs echo -n)/g > fstab",
      "cat ${var.project_root}/image/cmdline.txt | sed s/uuid/$(od -A n -X -j 440 -N 4 harness.img | xargs echo -n)/g > cmdline.txt"
    ]
  }

  provisioner "file" {
    source = "fstab"
    destination = "/etc/fstab"
    generated = true
  }

  provisioner "file" {
    source = "cmdline.txt"
    destination = "/boot/cmdline.txt"
    generated = true
  }
}
