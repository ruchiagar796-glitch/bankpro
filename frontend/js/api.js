// api.js - the ONLY place that talks to the C++ backend.
// Every function returns the "data" part of a successful JSON response, or throws
// an ApiError whose message comes from the backend (so the user sees the real reason).
'use strict';

class ApiError extends Error {
  constructor(message, status, code) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

const api = (() => {
  // Same origin as the page: the C++ server serves both the page and the API.
  const BASE = '/api';

  async function request(method, path, body) {
    const options = { method, headers: { Accept: 'application/json' } };
    if (body !== undefined) {
      options.headers['Content-Type'] = 'application/json';
      options.body = JSON.stringify(body);
    }
    let response;
    try {
      response = await fetch(BASE + path, options);
    } catch (e) {
      throw new ApiError('Cannot reach the bank server. Is bank_server running?', 0, 'NETWORK');
    }
    let payload = null;
    try { payload = await response.json(); } catch (e) { /* non-JSON response */ }
    if (!payload) throw new ApiError('Server returned an unexpected response (HTTP ' + response.status + ')', response.status, 'BAD_RESPONSE');
    if (!response.ok || payload.success === false) {
      const err = payload.error || {};
      throw new ApiError(err.message || 'Request failed', response.status, err.code || 'ERROR');
    }
    return payload; // { success, message, data }
  }

  const q = (params) => {
    const s = new URLSearchParams(params).toString();
    return s ? '?' + s : '';
  };

  return {
    dashboard: () => request('GET', '/dashboard'),
    accountTypes: () => request('GET', '/account-types'),
    listAccounts: (search) => request('GET', '/accounts' + q(search ? { search } : {})),
    sortedAccounts: (by, order) => request('GET', '/accounts/sorted' + q({ by, order })),
    getAccount: (n) => request('GET', '/accounts/' + encodeURIComponent(n)),
    createAccount: (body) => request('POST', '/accounts', body),
    closeAccount: (n) => request('DELETE', '/accounts/' + encodeURIComponent(n)),
    deposit: (body) => request('POST', '/transactions/deposit', body),
    withdraw: (body) => request('POST', '/transactions/withdraw', body),
    transfer: (body) => request('POST', '/transactions/transfer', body),
    transactions: (limit) => request('GET', '/transactions' + q(limit ? { limit } : {})),
    accountTransactions: (n) => request('GET', '/transactions/account/' + encodeURIComponent(n)),
    nextReversible: () => request('GET', '/transactions/reversible'),
    reverse: (transactionId) => request('POST', '/transactions/reverse', transactionId ? { transactionId } : {}),
    queue: () => request('GET', '/queue'),
    issueToken: (body) => request('POST', '/queue', body),
    serveNext: () => request('POST', '/queue/serve', {}),
  };
})();
