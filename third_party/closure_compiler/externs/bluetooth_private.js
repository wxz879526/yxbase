// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated by:
//   ./tools/json_schema_compiler/compiler.py.
// NOTE: The format of types has changed. 'FooType' is now
//   'chrome.bluetoothPrivate.FooType'.
// Please run the closure compiler before committing changes.
// See https://chromium.googlesource.com/chromium/src/+/master/docs/closure_compilation.md

// IMPORTANT:
// s/chrome.bluetoothPrivate.bluetooth.Device/chrome.bluetooth.Device/

/** @fileoverview Externs generated from namespace: bluetoothPrivate */

/**
 * @const
 */
chrome.bluetoothPrivate = {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-PairingEventType
 */
chrome.bluetoothPrivate.PairingEventType = {
  REQUEST_PINCODE: 'requestPincode',
  DISPLAY_PINCODE: 'displayPincode',
  REQUEST_PASSKEY: 'requestPasskey',
  DISPLAY_PASSKEY: 'displayPasskey',
  KEYS_ENTERED: 'keysEntered',
  CONFIRM_PASSKEY: 'confirmPasskey',
  REQUEST_AUTHORIZATION: 'requestAuthorization',
  COMPLETE: 'complete',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-ConnectResultType
 */
chrome.bluetoothPrivate.ConnectResultType = {
  ALREADY_CONNECTED: 'alreadyConnected',
  AUTH_CANCELED: 'authCanceled',
  AUTH_FAILED: 'authFailed',
  AUTH_REJECTED: 'authRejected',
  AUTH_TIMEOUT: 'authTimeout',
  FAILED: 'failed',
  IN_PROGRESS: 'inProgress',
  SUCCESS: 'success',
  UNKNOWN_ERROR: 'unknownError',
  UNSUPPORTED_DEVICE: 'unsupportedDevice',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-PairingResponse
 */
chrome.bluetoothPrivate.PairingResponse = {
  CONFIRM: 'confirm',
  REJECT: 'reject',
  CANCEL: 'cancel',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-TransportType
 */
chrome.bluetoothPrivate.TransportType = {
  LE: 'le',
  BREDR: 'bredr',
  DUAL: 'dual',
};

/**
 * @typedef {{
 *   pairing: !chrome.bluetoothPrivate.PairingEventType,
 *   device: !chrome.bluetooth.Device,
 *   pincode: (string|undefined),
 *   passkey: (number|undefined),
 *   enteredKey: (number|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-PairingEvent
 */
chrome.bluetoothPrivate.PairingEvent;

/**
 * @typedef {{
 *   name: (string|undefined),
 *   powered: (boolean|undefined),
 *   discoverable: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-NewAdapterState
 */
chrome.bluetoothPrivate.NewAdapterState;

/**
 * @typedef {{
 *   device: !chrome.bluetooth.Device,
 *   response: !chrome.bluetoothPrivate.PairingResponse,
 *   pincode: (string|undefined),
 *   passkey: (number|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-SetPairingResponseOptions
 */
chrome.bluetoothPrivate.SetPairingResponseOptions;

/**
 * @typedef {{
 *   transport: (!chrome.bluetoothPrivate.TransportType|undefined),
 *   uuids: ((string|!Array<string>)|undefined),
 *   rssi: (number|undefined),
 *   pathloss: (number|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-DiscoveryFilter
 */
chrome.bluetoothPrivate.DiscoveryFilter;

/**
 * Changes the state of the Bluetooth adapter.
 * @param {!chrome.bluetoothPrivate.NewAdapterState} adapterState
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-setAdapterState
 */
chrome.bluetoothPrivate.setAdapterState = function(adapterState, callback) {};

/**
 * @param {!chrome.bluetoothPrivate.SetPairingResponseOptions} options
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-setPairingResponse
 */
chrome.bluetoothPrivate.setPairingResponse = function(options, callback) {};

/**
 * Tears down all connections to the given device.
 * @param {string} deviceAddress
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-disconnectAll
 */
chrome.bluetoothPrivate.disconnectAll = function(deviceAddress, callback) {};

/**
 * Forgets the given device.
 * @param {string} deviceAddress
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-forgetDevice
 */
chrome.bluetoothPrivate.forgetDevice = function(deviceAddress, callback) {};

/**
 * Set or clear discovery filter.
 * @param {!chrome.bluetoothPrivate.DiscoveryFilter} discoveryFilter
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-setDiscoveryFilter
 */
chrome.bluetoothPrivate.setDiscoveryFilter = function(discoveryFilter, callback) {};

/**
 * Connects to the given device. This will only throw an error if the device
 * address is invalid or the device is already connected. Otherwise this will
 * succeed and invoke |callback| with ConnectResultType.
 * @param {string} deviceAddress
 * @param {function(!chrome.bluetoothPrivate.ConnectResultType):void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-connect
 */
chrome.bluetoothPrivate.connect = function(deviceAddress, callback) {};

/**
 * Pairs the given device.
 * @param {string} deviceAddress
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#method-pair
 */
chrome.bluetoothPrivate.pair = function(deviceAddress, callback) {};

/**
 * Fired when a pairing event occurs.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#event-onPairing
 */
chrome.bluetoothPrivate.onPairing;
