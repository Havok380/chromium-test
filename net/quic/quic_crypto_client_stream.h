// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;

namespace test {
class CryptoTestUtils;
}  // namespace test

class NET_EXPORT_PRIVATE QuicCryptoClientStream : public QuicCryptoStream {
 public:
  QuicCryptoClientStream(QuicSession* session, const string& server_hostname);
  virtual ~QuicCryptoClientStream();

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  // Performs a crypto handshake with the server. Returns true if the crypto
  // handshake is started successfully.
  // TODO(agl): this should probably return void.
  virtual bool CryptoConnect();

  const QuicNegotiatedParameters& negotiated_params() const;
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params() const;

 private:
  friend class test::CryptoTestUtils;

  enum State {
    STATE_IDLE,
    STATE_SEND_CHLO,
    STATE_RECV_REJ,
    STATE_RECV_SHLO,
  };

  // DoHandshakeLoop performs a step of the handshake state machine. Note that
  // |in| is NULL for the first call.
  void DoHandshakeLoop(const CryptoHandshakeMessage* in);

  State next_state_;

  QuicConfig config_;
  QuicCryptoClientConfig crypto_config_;

  QuicNegotiatedParameters negotiated_params_;
  QuicCryptoNegotiatedParameters crypto_negotiated_params_;

  bool decrypter_pushed_;

  // Client's connection nonce (4-byte timestamp + 28 random bytes)
  std::string nonce_;
  // Server's hostname
  std::string server_hostname_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
