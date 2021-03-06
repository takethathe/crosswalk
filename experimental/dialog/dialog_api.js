// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extension._setupExtensionInternal();
var internal = extension._internal;

// FIXME(tmpsantos): Simple methods like this should be automatically
// created from the JSON Schema generated by the IDL file.
exports.showOpenDialog = function(arg1, arg2, arg3, arg4, arg5, callback) {
  internal.postMessage('showOpenDialog', [arg1, arg2, arg3, arg4, arg5], callback);
};

exports.showSaveDialog = function(arg1, arg2, arg3, callback) {
  internal.postMessage('showSaveDialog', [arg1, arg2, arg3], callback);
};
