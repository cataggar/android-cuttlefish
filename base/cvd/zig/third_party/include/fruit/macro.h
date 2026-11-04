#pragma once

#define ANNOTATED(Annotation, ...) __VA_ARGS__
#define INJECT(Signature)                     \
  using FruitInjectSignature = Signature;     \
  Signature
