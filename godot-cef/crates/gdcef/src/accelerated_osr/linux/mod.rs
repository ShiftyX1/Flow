//! Linux-specific accelerated OSR implementation.
//!
//! On Linux, we use Vulkan with DMA-BUF external memory extensions to import
//! shared textures from CEF's compositor process.

mod vulkan;

use super::RenderBackend;
use cef::AcceleratedPaintInfo;
use godot::global::{godot_print, godot_warn};
use godot::prelude::*;

const NVIDIA_VENDOR_ID: u32 = 0x10de;

pub fn get_godot_gpu_device_ids() -> Option<(u32, u32)> {
    vulkan::get_godot_gpu_device_ids()
}

pub struct GodotTextureImporter {
    vulkan_importer: vulkan::VulkanTextureImporter,
}

impl GodotTextureImporter {
    pub fn new() -> Option<Self> {
        let render_backend = RenderBackend::detect();

        if !render_backend.supports_accelerated_osr() {
            godot_warn!(
                "[AcceleratedOSR/Linux] Render backend {:?} does not support accelerated OSR",
                render_backend
            );
            return None;
        }

        match render_backend {
            RenderBackend::Vulkan => {
                let (supported, reason) = vulkan_support_diagnostic();
                if !supported {
                    godot_warn!(
                        "[AcceleratedOSR/Linux] Vulkan accelerated OSR unavailable: {}",
                        reason
                    );
                    return None;
                }

                let vulkan_importer = vulkan::VulkanTextureImporter::new()?;
                godot_print!("[AcceleratedOSR/Linux] Using Vulkan backend with DMA-BUF");
                Some(Self { vulkan_importer })
            }
            _ => {
                godot_warn!(
                    "[AcceleratedOSR/Linux] Unsupported render backend: {:?}",
                    render_backend
                );
                None
            }
        }
    }

    pub fn queue_copy(&mut self, info: &AcceleratedPaintInfo) -> Result<(), String> {
        self.vulkan_importer.queue_copy(info)
    }

    pub fn process_pending_copy(&mut self, dst_rd_rid: Rid) -> Result<(), String> {
        self.vulkan_importer.process_pending_copy(dst_rd_rid)
    }

    pub fn wait_for_copy(&mut self) -> Result<(), String> {
        self.vulkan_importer.wait_for_copy()
    }
}

pub fn is_supported() -> bool {
    let render_backend = RenderBackend::detect();
    if !render_backend.supports_accelerated_osr() {
        return false;
    }

    match render_backend {
        RenderBackend::Vulkan => vulkan_support_diagnostic().0,
        _ => false,
    }
}

unsafe impl Send for GodotTextureImporter {}
unsafe impl Sync for GodotTextureImporter {}

pub fn vulkan_support_diagnostic() -> (bool, String) {
    match vulkan_support_probe() {
        Ok(reason) => (true, reason),
        Err(reason) => (false, reason),
    }
}

fn vulkan_support_probe() -> Result<String, String> {
    let Some((vendor_id, _device_id)) = vulkan::get_godot_gpu_device_ids() else {
        return Ok(
            "Vulkan backend supports accelerated OSR; GPU vendor could not be determined"
                .to_string(),
        );
    };

    if vendor_id != NVIDIA_VENDOR_ID {
        return Ok(format!(
            "Vulkan backend on GPU vendor 0x{vendor_id:04x} supports accelerated OSR"
        ));
    }

    Ok("NVIDIA Vulkan backend assumes nvidia-drm.modeset is enabled".to_string())
}
