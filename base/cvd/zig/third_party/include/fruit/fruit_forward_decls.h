#pragma once

namespace fruit {

template <typename... Params>
class Component;

class PartialComponent;

template <typename... Params>
class Injector;

template <typename Annotation, typename T>
struct Annotated {};

template <typename... Params>
struct Required {};

}  // namespace fruit
