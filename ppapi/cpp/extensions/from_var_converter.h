// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_
#define PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/var_array_dev.h"
#include "ppapi/cpp/dev/var_dictionary_dev.h"
#include "ppapi/cpp/extensions/optional.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/var.h"

namespace pp {
namespace ext {
namespace internal {

template <class T>
class FromVarConverterBase {
 public:
  T& value() { return value_; }

 protected:
  FromVarConverterBase() : value_() {
  }

  explicit FromVarConverterBase(const T& value) : value_(value) {
  }

  ~FromVarConverterBase() {
  }

  T value_;
};

template <class T>
class FromVarConverter : public FromVarConverterBase<T> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    bool succeeded = FromVarConverterBase<T>::value_.Populate(var);
    // Suppress unused variable warnings.
    static_cast<void>(succeeded);
    PP_DCHECK(succeeded);
  }
};

template <class T>
class FromVarConverter<Optional<T> >
    : public FromVarConverterBase<Optional<T> > {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    if (var.type == PP_VARTYPE_UNDEFINED) {
      FromVarConverterBase<Optional<T> >::value_.Reset();
    } else {
      FromVarConverter<T> converter(var);
      FromVarConverterBase<Optional<T> >::value_ = converter.value();
    }
  }
};

template <>
class FromVarConverter<bool> : public FromVarConverterBase<bool> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<bool>::value_ = Var(var).AsBool();
  }
};

template <>
class FromVarConverter<int32_t> : public FromVarConverterBase<int32_t> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<int32_t>::value_ = Var(var).AsInt();
  }
};

template <>
class FromVarConverter<double> : public FromVarConverterBase<double> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<double>::value_ = Var(var).AsDouble();
  }
};

template <>
class FromVarConverter<std::string> : public FromVarConverterBase<std::string> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<std::string>::value_ = Var(var).AsString();
  }
};

template <>
class FromVarConverter<Var> : public FromVarConverterBase<Var> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<Var>::value_ = Var(var);
  }
};

template <>
class FromVarConverter<VarArray_Dev>
    : public FromVarConverterBase<VarArray_Dev> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<VarArray_Dev>::value_ = Var(var);
  }
};

template <>
class FromVarConverter<VarDictionary_Dev>
    : public FromVarConverterBase<VarDictionary_Dev> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(var);
  }

  ~FromVarConverter() {
  }

  void Set(const PP_Var& var) {
    FromVarConverterBase<VarDictionary_Dev>::value_ = Var(var);
  }
};

}  // namespace internal
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_
