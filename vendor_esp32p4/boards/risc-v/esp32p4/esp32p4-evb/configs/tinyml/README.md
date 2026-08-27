# ESP32-P4 TinyML Configuration

This configuration enables TensorFlow Lite Micro support for edge AI applications on the ESP32-P4 EV Board.

## Features

- **TensorFlow Lite Micro Runtime**: Optimized ML inference engine for microcontrollers
- **PSRAM Support**: 32MB external PSRAM for model data and tensor arena
- **Common ML Operators**: Pre-configured operators for typical neural network layers
- **Demo Application**: Built-in sine wave prediction demo

## Building

To build with TinyML support:

```bash
# Configure with TinyML defconfig
./tools/configure.sh esp32p4-evb:tinyml

# Build the firmware
make -j$(nproc)
```

## Running the Demo

After flashing the firmware, run the ML demo:

```bash
# In NuttX NSH shell
nsh> esp32p4_ml_demo -h    # Show help
nsh> esp32p4_ml_demo -s    # Run sine wave prediction test
nsh> esp32p4_ml_demo -a    # Show arena information
nsh> esp32p4_ml_demo       # Run all demos
```

## Memory Configuration

TinyML requires PSRAM for optimal performance:

| Parameter | Value | Description |
|-----------|-------|-------------|
| `CONFIG_ESP32P4_PSRAM` | y | Enable PSRAM support |
| `CONFIG_ESP32P4_PSRAM_SIZE` | 32 | 32MB PSRAM |
| `CONFIG_MM_REGIONS` | 2 | Multiple memory regions (SRAM + PSRAM) |
| `CONFIG_ESP32P4_TFLITE_ARENA_SIZE` | 128 | 128KB tensor arena |

## TFLite Micro Operators

The following operators are enabled by default:

| Operator | Description |
|----------|-------------|
| Conv2D | Convolutional layer |
| Dense | Fully connected layer |
| MaxPool2D | Max pooling layer |
| ReLU | ReLU activation |
| Softmax | Softmax activation |
| Quantize | Quantization operations |
| Reshape | Tensor reshape |
| Add | Addition operation |
| Mul | Multiplication operation |

To add more operators, enable additional `CONFIG_ESP32P4_TINYML_OPS_*` options in menuconfig.

## Creating Custom Models

1. Train your model using TensorFlow
2. Convert to TFLite format:
   ```python
   converter = tf.lite.TFLiteConverter.from_saved_model('model_dir')
   converter.optimizations = [tf.lite.Optimize.DEFAULT]
   tflite_model = converter.convert()
   ```
3. Convert to C array using `xxd`:
   ```bash
   xxd -i model.tflite > model_data.cc
   ```
4. Include the model data in your application

## Troubleshooting

### Out of Memory

If you encounter memory errors:
- Increase `CONFIG_ESP32P4_TFLITE_ARENA_SIZE`
- Reduce model complexity
- Enable quantization to reduce model size

### Build Errors

Ensure you have the ESP-IDF TFLite Micro component:
```bash
# Check component exists
ls -la components/esp-tflite-micro/
```

## References

- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP-IDF TFLite Micro Component](https://github.com/espressif/esp-tflite-micro)
- [ESP32-P4 Technical Reference](https://www.espressif.com/en/products/socs/esp32-p4)
