#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace Image {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Exception;
using v8::Boolean;

// Bicubic interpolation helper functions
float CubicInterpolate(float p[4], float x) {
  return p[1] + 0.5 * x * (p[2] - p[0] + x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] + x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
}

float BicubicInterpolate(float p[4][4], float x, float y) {
  float arr[4];
  arr[0] = CubicInterpolate(p[0], y);
  arr[1] = CubicInterpolate(p[1], y);
  arr[2] = CubicInterpolate(p[2], y);
  arr[3] = CubicInterpolate(p[3], y);
  return CubicInterpolate(arr, x);
}

class Image : public node::ObjectWrap {
 public:
  static void Init(Local<Object> exports) {
    Isolate* isolate = exports->GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "Image").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype methods
    NODE_SET_PROTOTYPE_METHOD(tpl, "getWidth", GetWidth);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getHeight", GetHeight);
    NODE_SET_PROTOTYPE_METHOD(tpl, "resize", Resize);
    NODE_SET_PROTOTYPE_METHOD(tpl, "save", Save);
    NODE_SET_PROTOTYPE_METHOD(tpl, "load", Load);
    NODE_SET_PROTOTYPE_METHOD(tpl, "draw", Draw);

    Local<Function> constructor = tpl->GetFunction(context).ToLocalChecked();
    exports->Set(context, String::NewFromUtf8(isolate, "Image").ToLocalChecked(), constructor).Check();
  }

 private:
  explicit Image() : width_(0), height_(0), pixels_(nullptr) {}

  ~Image() {
    if (pixels_) {
      stbi_image_free(pixels_);
    }
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    if (args.IsConstructCall()) {
      Image* obj = new Image();
      if (args.Length() == 1 && args[0]->IsNumber()) {
        int size = args[0]->Int32Value(context).ToChecked();
        obj->width_ = size;
        obj->height_ = size;
        obj->pixels_ = (unsigned char*)malloc(size * size * 3);
        if (!obj->pixels_) {
          isolate->ThrowException(Exception::Error(
              String::NewFromUtf8(isolate, "Failed to allocate memory for image").ToLocalChecked()));
          delete obj;
          return;
        }
        // Initialize the image with a default color (e.g., black)
        memset(obj->pixels_, 0, size * size * 3);
      } else if (args.Length() == 2 && args[0]->IsNumber() && args[1]->IsNumber()) {
        int width = args[0]->Int32Value(context).ToChecked();
        int height = args[1]->Int32Value(context).ToChecked();
        obj->width_ = width;
        obj->height_ = height;
        obj->pixels_ = (unsigned char*)malloc(width * height * 3);
        if (!obj->pixels_) {
          isolate->ThrowException(Exception::Error(
              String::NewFromUtf8(isolate, "Failed to allocate memory for image").ToLocalChecked()));
          delete obj;
          return;
        }
        // Initialize the image with a default color (e.g., black)
        memset(obj->pixels_, 0, width * height * 3);
      } else {
        obj->width_ = 0;
        obj->height_ = 0;
        obj->pixels_ = nullptr;
      }
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
    } else {
      const int argc = args.Length();
      Local<Value> argv[argc];
      for (int i = 0; i < argc; ++i) {
        argv[i] = args[i];
      }
      Local<Function> cons = args.Data().As<Function>();
      Local<Object> result = cons->NewInstance(context, argc, argv).ToLocalChecked();
      args.GetReturnValue().Set(result);
    }
  }

  static void GetWidth(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    args.GetReturnValue().Set(Number::New(isolate, obj->width_));
  }

  static void GetHeight(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    args.GetReturnValue().Set(Number::New(isolate, obj->height_));
  }

  static void Resize(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    if (args.Length() < 1 || !args[0]->IsNumber()) {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
      return;
    }

    double widthRatio = args[0]->NumberValue(context).ToChecked();
    double heightRatio = widthRatio; // Default to the same ratio for height

    if (args.Length() > 1 && args[1]->IsNumber()) {
      heightRatio = args[1]->NumberValue(context).ToChecked();
    }

    int newWidth = static_cast<int>(obj->width_ * widthRatio);
    int newHeight = static_cast<int>(obj->height_ * heightRatio);

    unsigned char* newPixels = (unsigned char*)malloc(newWidth * newHeight * 3);
    if (!newPixels) {
      isolate->ThrowException(Exception::Error(
          String::NewFromUtf8(isolate, "Failed to allocate memory for resized image").ToLocalChecked()));
      return;
    }

    // Bicubic interpolation
    for (int y = 0; y < newHeight; ++y) {
      for (int x = 0; x < newWidth; ++x) {
        float gx = x / (float)newWidth * (obj->width_ - 1);
        float gy = y / (float)newHeight * (obj->height_ - 1);
        int gxi = (int)gx;
        int gyi = (int)gy;

        float result[3] = {0, 0, 0};
        for (int c = 0; c < 3; ++c) {
          float p[4][4];
          for (int m = -1; m <= 2; ++m) {
            for (int n = -1; n <= 2; ++n) {
              int px = std::min(std::max(gxi + m, 0), obj->width_ - 1);
              int py = std::min(std::max(gyi + n, 0), obj->height_ - 1);
              p[m + 1][n + 1] = obj->pixels_[(py * obj->width_ + px) * 3 + c];
            }
          }
          result[c] = BicubicInterpolate(p, gx - gxi, gy - gyi);
        }

        int destIndex = (y * newWidth + x) * 3;
        newPixels[destIndex] = std::min(std::max((int)result[0], 0), 255);
        newPixels[destIndex + 1] = std::min(std::max((int)result[1], 0), 255);
        newPixels[destIndex + 2] = std::min(std::max((int)result[2], 0), 255);
      }
    }

    // Free the old pixel data and update the object
    if (obj->pixels_) {
      stbi_image_free(obj->pixels_);
    }
    obj->pixels_ = newPixels;
    obj->width_ = newWidth;
    obj->height_ = newHeight;

    args.GetReturnValue().Set(Boolean::New(isolate, true));
  }

  static void Save(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
      return;
    }

    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    String::Utf8Value filename(isolate, args[0]);
    String::Utf8Value type(isolate, args[1]);

    if (!obj->pixels_) {
      isolate->ThrowException(Exception::Error(
          String::NewFromUtf8(isolate, "No image data to save").ToLocalChecked()));
      return;
    }

    std::string typeStr(*type);
    std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Validate image type
    if (typeStr != "png" && typeStr != "jpeg" && typeStr != "jpg") {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Unsupported image type").ToLocalChecked()));
      return;
    }

    // Default quality settings
    int pngCompressionLevel = 0;  // Default PNG compression (0-9)
    int jpegQuality = 100;        // Default JPEG quality (0-100)

    // Parse options if provided
    if (args.Length() > 2 && args[2]->IsObject()) {
      Local<Object> options = args[2].As<Object>();
      
      if (typeStr == "png") {
        Local<String> compressionKey = String::NewFromUtf8(isolate, "compressionLevel").ToLocalChecked();
        if (options->Has(context, compressionKey).ToChecked()) {
          pngCompressionLevel = options->Get(context, compressionKey).ToLocalChecked()->Int32Value(context).ToChecked();
          pngCompressionLevel = std::max(0, std::min(pngCompressionLevel, 9));  // Clamp to 0-9
        }
      } else if (typeStr == "jpeg" || typeStr == "jpg") {
        Local<String> qualityKey = String::NewFromUtf8(isolate, "quality").ToLocalChecked();
        if (options->Has(context, qualityKey).ToChecked()) {
          jpegQuality = options->Get(context, qualityKey).ToLocalChecked()->Int32Value(context).ToChecked();
          jpegQuality = std::max(0, std::min(jpegQuality, 100));  // Clamp to 0-100
        }
      }
    }

    int result = 0;
    if (typeStr == "png") {
      stbi_write_png_compression_level = pngCompressionLevel;
      result = stbi_write_png(*filename, obj->width_, obj->height_, 3, obj->pixels_, obj->width_ * 3);
    } else if (typeStr == "jpeg" || typeStr == "jpg") {
      result = stbi_write_jpg(*filename, obj->width_, obj->height_, 3, obj->pixels_, jpegQuality);
    }

    args.GetReturnValue().Set(Boolean::New(isolate, result != 0));
  }

  static void Load(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    if (args.Length() < 1 || !args[0]->IsString()) {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
      return;
    }

    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    String::Utf8Value filename(isolate, args[0]);

    int width, height, channels;
    unsigned char* data = stbi_load(*filename, &width, &height, &channels, 3);

    if (!data) {
      isolate->ThrowException(Exception::Error(
          String::NewFromUtf8(isolate, "Failed to load image").ToLocalChecked()));
      return;
    }

    // Free existing pixels if any
    if (obj->pixels_) {
      stbi_image_free(obj->pixels_);
    }

    // Update object properties
    obj->width_ = width;
    obj->height_ = height;
    obj->pixels_ = data;

    args.GetReturnValue().Set(Boolean::New(isolate, true));
  }

  static void Draw(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    if (args.Length() < 3 || !args[0]->IsObject() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
      return;
    }

    Image* obj = ObjectWrap::Unwrap<Image>(args.Holder());
    Image* other = ObjectWrap::Unwrap<Image>(args[0]->ToObject(context).ToLocalChecked());
    int x = args[1]->Int32Value(context).ToChecked();
    int y = args[2]->Int32Value(context).ToChecked();

    if (!obj->pixels_ || !other->pixels_) {
      isolate->ThrowException(Exception::Error(
          String::NewFromUtf8(isolate, "One or both images have no data").ToLocalChecked()));
      return;
    }

    // Perform the drawing operation
    for (int j = 0; j < other->height_; ++j) {
      for (int i = 0; i < other->width_; ++i) {
        int destX = x + i;
        int destY = y + j;
        
        // Check if the pixel is within the bounds of the destination image
        if (destX >= 0 && destX < obj->width_ && destY >= 0 && destY < obj->height_) {
          int destIndex = (destY * obj->width_ + destX) * 3;
          int srcIndex = (j * other->width_ + i) * 3;
          
          // Simple alpha blending (assuming the last channel is alpha)
          float alpha = other->pixels_[srcIndex + 2] / 255.0f;
          for (int c = 0; c < 3; ++c) {
            obj->pixels_[destIndex + c] = (unsigned char)(
              obj->pixels_[destIndex + c] * (1 - alpha) + other->pixels_[srcIndex + c] * alpha
            );
          }
        }
      }
    }

    args.GetReturnValue().Set(Boolean::New(isolate, true));
  }

  int width_;
  int height_;
  unsigned char* pixels_;
};

void Initialize(Local<Object> exports) {
  Image::Init(exports);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace Image
