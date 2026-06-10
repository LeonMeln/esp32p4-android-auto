/// Turns a GIF into a sequence of JPEG boot-splash frames for the head unit.
///
/// The head unit's new animated boot splash is a folder of JPEG frames named
/// `<index>-<durationMs>.jpg` under `/vescfs/splash` (see firmware
/// main/splash_screen.c), decoded on-device by the P4 hardware JPEG decoder.
/// This module decodes a GIF (with per-frame durations), downscales each frame
/// to fit the 800×480 panel, re-encodes it as JPEG, and caps the frame count.
///
/// The heavy decode/encode runs in a background isolate via [compute] so the UI
/// stays responsive.
library;

import 'package:flutter/foundation.dart';
import 'package:image/image.dart' as img;

/// User-facing landscape panel size. Every frame is produced at exactly this
/// size (cover-fill: scaled to fill, centre-cropped) so the firmware can draw
/// it full-screen. The firmware rotates 270° into the panel-native 480×800 via
/// PPA — the app does NOT rotate.
const int kSplashW = 800;
const int kSplashH = 480;

/// Mirror of MAX_FRAMES / MIN_FRAME_MS / MAX_FRAME_MS in firmware splash_screen.c.
const int kSplashMaxFrames = 120;
const int kSplashMinDurMs = 20;
const int kSplashMaxDurMs = 60000;
const int kSplashJpegQuality = 80;

/// One encoded frame: JPEG bytes + how long to hold it (milliseconds).
class SplashFrame {
  final Uint8List jpeg;
  final int durMs;
  const SplashFrame(this.jpeg, this.durMs);
}

/// Result of slicing a GIF: the encoded frames plus their (uniform) dimensions.
class SplashBuildResult {
  final List<SplashFrame> frames;
  final int width;
  final int height;
  const SplashBuildResult(this.frames, this.width, this.height);
}

/// Thrown when a GIF can't be decoded into usable frames. [key] is an i18n key.
class SplashBuildException implements Exception {
  final String key;
  const SplashBuildException(this.key);
  @override
  String toString() => 'SplashBuildException($key)';
}

/// Decode [gifBytes] and produce the JPEG frame sequence. Runs in an isolate.
Future<SplashBuildResult> buildSplashFrames(Uint8List gifBytes) =>
    compute(_buildSplashFramesSync, gifBytes);

SplashBuildResult _buildSplashFramesSync(Uint8List gifBytes) {
  final decoded = img.decodeGif(gifBytes);
  if (decoded == null || decoded.frames.isEmpty) {
    throw const SplashBuildException('splash.err.decode');
  }

  // The image package composites GIF frames (handles disposal/blending), so
  // each entry in .frames is a full image. frameDuration is in milliseconds.
  final src = decoded.frames;
  final n = src.length;

  // Even-sample down to the frame cap, folding dropped frames' durations into
  // the next kept frame so total playback time stays roughly the same.
  final keepEvery = (n + kSplashMaxFrames - 1) ~/ kSplashMaxFrames; // >= 1
  final frames = <SplashFrame>[];
  int carry = 0;
  for (int i = 0; i < n; i++) {
    final d = src[i].frameDuration;
    if (d > 0) carry += d;
    final isLast = i == n - 1;
    if (i % keepEvery != 0 && !isLast) continue;

    // MUST be yuv420 (4:2:0): the P4 hardware JPEG decoder hardwires a
    // YUV420→RGB565 colour-space conversion for RGB565 output (see
    // jpeg_decode.c jpeg_dec_config_dma_csc — there is no YUV444→RGB565 path).
    // The image package defaults to yuv444, which the HW then mis-decodes into
    // chroma artifacts. yuv420 matches what Android album-art JPEGs use (the
    // known-good music_info_view path).
    final jpeg = img.encodeJpg(_fitLetterbox(src[i]),
        quality: kSplashJpegQuality, chroma: img.JpegChroma.yuv420);

    var dur = carry;
    carry = 0;
    if (dur < kSplashMinDurMs) dur = kSplashMinDurMs;
    if (dur > kSplashMaxDurMs) dur = kSplashMaxDurMs;
    frames.add(SplashFrame(jpeg, dur));
  }

  if (frames.isEmpty) throw const SplashBuildException('splash.err.decode');
  return SplashBuildResult(frames, kSplashW, kSplashH);
}

/// Fit [frame] inside kSplashW×kSplashH preserving aspect ratio (the whole image
/// is kept — nothing cropped), centred on a black canvas. Mismatched aspect ends
/// up with black bars on the sides (pillarbox) or top/bottom (letterbox).
img.Image _fitLetterbox(img.Image src) {
  // GIF frames are palette-backed (indexed colour). copyResize/encodeJpg on a
  // palette image interpolate the palette INDICES, which produces salt-and-
  // pepper garbage (a clean white background turns into noise). De-palettize to
  // packed 8-bit RGB first — this also drops the alpha channel we don't need.
  final frame = (src.hasPalette || src.numChannels != 3)
      ? src.convert(format: img.Format.uint8, numChannels: 3)
      : src;
  if (frame.width == kSplashW && frame.height == kSplashH) return frame;
  // CONTAIN: smaller ratio so the whole frame fits inside the screen box.
  final scale = (kSplashW / frame.width) < (kSplashH / frame.height)
      ? kSplashW / frame.width
      : kSplashH / frame.height;
  final rw = (frame.width * scale).round().clamp(1, kSplashW);
  final rh = (frame.height * scale).round().clamp(1, kSplashH);
  final resized = img.copyResize(frame,
      width: rw, height: rh, interpolation: img.Interpolation.average);
  if (rw == kSplashW && rh == kSplashH) return resized;
  // Centre the fitted frame on a black kSplashW×kSplashH canvas (the bars).
  final canvas = img.Image(width: kSplashW, height: kSplashH, numChannels: 3);
  img.fill(canvas, color: img.ColorRgb8(0, 0, 0));
  img.compositeImage(canvas, resized,
      dstX: (kSplashW - rw) ~/ 2, dstY: (kSplashH - rh) ~/ 2);
  return canvas;
}
