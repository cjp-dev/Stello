//! The engine for the web app's Web Worker. The search API comes in phase 3.

use wasm_bindgen::prelude::wasm_bindgen;

/// The engine's version, for the worker's sanity check.
#[wasm_bindgen]
pub fn version() -> String {
    stello_core::VERSION.to_string()
}
