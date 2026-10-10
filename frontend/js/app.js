// app.js - page logic. Contains NO banking rules: it validates input for convenience,
// calls api.js, and displays whatever the C++ backend returns.
'use strict';

const $ = (sel, root = document) => root.querySelector(sel);

const TITLES = {
  dashboard: 'Dashboard', create: 'Create Account', accounts: 'All Accounts',
  details: 'Account Details', deposit: 'Deposit', withdraw: 'Withdraw',
  transfer: 'Transfer Funds', history: 'Transaction History',
  queue: 'Customer Service Queue', sorted: 'Sorted Accounts', about: 'About Project',
};

// ---------- small helpers ----------
function formatINR(text) {            // "1234567.5" -> "Rs 12,34,567.50" (Indian grouping, no float maths)
  const [whole, frac = '00'] = String(text).split('.');
  let head = whole, tail = '';
  if (whole.length > 3) {
    tail = ',' + whole.slice(-3);
    head = whole.slice(0, -3).replace(/\B(?=(\d{2})+(?!\d))/g, ',');
  }
  return 'Rs ' + head + tail + '.' + (frac + '00').slice(0, 2);
}

function el(tag, text, className) {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;   // textContent => no HTML injection
  if (className) node.className = className;
  return node;
}

function renderTable(table, columns, rows, emptyText) {
  table.replaceChildren();
  if (rows.length === 0) {
    const caption = el('caption', emptyText, 'empty');
    table.appendChild(caption);
    return;
  }
  const thead = el('thead'); const hr = el('tr');
  columns.forEach((c) => { const th = el('th', c.label, c.num ? 'num' : ''); th.scope = 'col'; hr.appendChild(th); });
  thead.appendChild(hr); table.appendChild(thead);
  const tbody = el('tbody');
  rows.forEach((row) => {
    const tr = el('tr');
    columns.forEach((c) => {
      const td = el('td', '', [c.num ? 'num' : '', c.cls ? c.cls(row) : ''].join(' ').trim());
      const content = c.render(row);
      if (content instanceof Node) td.appendChild(content); else td.textContent = content;
      tr.appendChild(td);
    });
    tbody.appendChild(tr);
  });
  table.appendChild(tbody);
}

function toast(message, isError = false) {
  const t = el('div', message, 'toast' + (isError ? ' error' : ''));
  $('#toasts').appendChild(t);
  setTimeout(() => t.remove(), isError ? 7000 : 4500);
}

function confirmDialog(title, text, okLabel) {
  const dlg = $('#confirmDialog');
  $('#confirmTitle').textContent = title;
  $('#confirmText').textContent = text;
  $('#confirmOk').textContent = okLabel || 'Confirm';
  return new Promise((resolve) => {
    dlg.addEventListener('close', () => resolve(dlg.returnValue === 'ok'), { once: true });
    dlg.showModal();
  });
}

const reAccount = /^\d{1,15}$/;
const reAmount = /^\d{1,12}(\.\d{1,2})?$/;

// Runs an async action with the submit button disabled; shows backend errors in the form.
async function submitting(form, action) {
  const btn = $('button[type=submit]', form);
  const errBox = $('.form-error', form);
  if (errBox) errBox.textContent = '';
  if (btn) btn.disabled = true;
  try { await action(); }
  catch (e) { if (errBox) errBox.textContent = e.message; toast(e.message, true); }
  finally { if (btn) btn.disabled = false; }
}

function formError(form, message) {
  const box = $('.form-error', form);
  if (box) box.textContent = message;
  return false;
}

// ---------- transaction table ----------
const TYPE_LABEL = { deposit: 'Deposit', withdrawal: 'Withdrawal', transfer_out: 'Transfer out', transfer_in: 'Transfer in', reversal: 'Reversal' };
const isDebit = (t) => t.type === 'withdrawal' || t.type === 'transfer_out' || (t.type === 'reversal' && t.description.includes('of deposit'));

function txColumns() {
  return [
    { label: 'ID', render: (t) => '#' + t.id },
    { label: 'Account', render: (t) => String(t.accountNumber) },
    { label: 'Type', render: (t) => el('span', TYPE_LABEL[t.type] || t.type, 'badge') },
    { label: 'Amount', num: true, cls: (t) => (isDebit(t) ? 'debit' : 'credit'), render: (t) => (isDebit(t) ? '- ' : '+ ') + formatINR(t.amount) },
    { label: 'Balance after', num: true, render: (t) => formatINR(t.balanceAfter) },
    { label: 'Date', render: (t) => t.timestamp },
    { label: 'Description', cls: () => 'wrap', render: (t) => t.description },
  ];
}

// ---------- views ----------
const views = {};

views.dashboard = async () => {
  const d = (await api.dashboard()).data;
  const cards = $('#dashCards'); cards.replaceChildren();
  [['Total accounts', String(d.totalAccounts), false], ['Total balance (all accounts)', formatINR(d.totalBalance), true],
   ['Total transactions', String(d.totalTransactions), false], ['Customers waiting', String(d.customersWaiting), false]]
    .forEach(([label, value, primary]) => {
      const c = el('div', '', 'card' + (primary ? ' primary' : ''));
      c.append(el('small', label), el('strong', value));
      cards.appendChild(c);
    });
  renderTable($('#dashTx'), txColumns(), d.recentTransactions, 'No transactions yet. Create an account to get started.');
  $('#dashNotice').textContent = d.notice;
};

views.create = async () => {
  const types = (await api.accountTypes()).data;
  const select = $('#cType');
  const showHint = () => {
    const t = types.find((x) => x.type === select.value);
    $('#cHint').textContent = t ? 'Minimum opening deposit: ' + formatINR(t.minimumOpeningDeposit) +
      '. Maximum per withdrawal: ' + formatINR(t.maxWithdrawalPerTransaction) + '.' : '';
  };
  select.onchange = showHint; showHint();
};

async function loadAccounts(search) {
  const accounts = (await api.listAccounts(search)).data;
  renderTable($('#accountsTable'), [
    { label: 'Account no.', render: (a) => String(a.accountNumber) },
    { label: 'Holder', render: (a) => a.holderName },
    { label: 'Type', render: (a) => el('span', a.accountType, 'badge') },
    { label: 'Balance', num: true, render: (a) => formatINR(a.balance) },
    { label: 'Opened', render: (a) => a.createdAt },
    { label: '', render: (a) => { const l = el('a', 'View details'); l.href = '#/details/' + a.accountNumber; return l; } },
  ], accounts, search ? 'No accounts match your search.' : 'No accounts yet. Use "Create Account" to open one.');
}
views.accounts = () => loadAccounts($('#searchBox').value.trim());

let currentAccount = null;
async function showDetails(number) {
  $('#detailsError').textContent = '';
  try {
    const a = (await api.getAccount(number)).data;
    const tx = (await api.accountTransactions(number)).data;
    currentAccount = a;
    $('#dNumber').value = a.accountNumber;
    $('#dName').textContent = a.holderName;
    $('#dMeta').textContent = a.accountType + ' account  |  No. ' + a.accountNumber + '  |  Opened ' + a.createdAt;
    $('#dBalance').textContent = formatINR(a.balance);
    $('#dDeposit').href = '#/deposit/' + a.accountNumber;
    $('#dWithdraw').href = '#/withdraw/' + a.accountNumber;
    renderTable($('#dTx'), txColumns(), tx, 'No transactions for this account.');
    $('#detailsBody').hidden = false;
  } catch (e) {
    currentAccount = null; $('#detailsBody').hidden = true;
    $('#detailsError').textContent = e.message;
  }
}
views.details = async (param) => {
  $('#detailsBody').hidden = true; currentAccount = null;
  if (param) await showDetails(param);
};

views.deposit = (param) => { if (param) $('#depAcc').value = param; };
views.withdraw = (param) => { if (param) $('#wdAcc').value = param; };
views.transfer = () => {};

async function loadHistory() {
  const acc = $('#hAcc').value.trim();
  $('#historyError').textContent = '';
  if (acc && !reAccount.test(acc)) { $('#historyError').textContent = 'Account number must be digits only'; return; }
  const tx = acc ? (await api.accountTransactions(acc)).data : (await api.transactions(100)).data;
  renderTable($('#historyTable'), txColumns(), tx, acc ? 'No transactions for this account.' : 'No transactions yet.');
}

async function loadReverseBox() {
  const box = $('#reverseBox'); box.replaceChildren();
  const r = (await api.nextReversible()).data;
  if (!r.transaction) { box.textContent = 'Nothing to reverse right now.'; return; }
  const t = r.transaction;
  const line = el('p', 'Next reversible: #' + t.id + ' - ' + (TYPE_LABEL[t.type]) + ' of ' + formatINR(t.amount) +
    ' on account ' + t.accountNumber + ' (' + t.timestamp + '). Stack size: ' + r.stackSize + '.');
  const btn = el('button', 'Reverse this transaction', 'btn danger'); btn.type = 'button';
  btn.onclick = async () => {
    const ok = await confirmDialog('Reverse transaction #' + t.id + '?',
      'A ' + TYPE_LABEL[t.type].toLowerCase() + ' of ' + formatINR(t.amount) + ' on account ' + t.accountNumber +
      ' will be undone. This adds a reversal record and cannot be undone again.', 'Reverse');
    if (!ok) return;
    try { const res = await api.reverse(t.id); toast(res.message); await views.history(); }
    catch (e) { toast(e.message, true); }
  };
  box.append(line, btn);
}
views.history = async () => { await Promise.all([loadHistory(), loadReverseBox()]); };

async function loadQueue() {
  const q = (await api.queue()).data;
  $('#qNext').textContent = 'T-' + String(q.nextTokenNumber).padStart(3, '0');
  renderTable($('#queueTable'), [
    { label: 'Token', render: (t) => 'T-' + String(t.token).padStart(3, '0') },
    { label: 'Customer', render: (t) => t.customerName },
    { label: 'Purpose', render: (t) => t.purpose || '-' },
    { label: 'Issued', render: (t) => t.issuedAt },
  ], q.waiting, 'Nobody is waiting.');
}
views.queue = () => loadQueue();

async function loadSorted() {
  const list = (await api.sortedAccounts($('#sortBy').value, $('#sortOrder').value)).data;
  renderTable($('#sortedTable'), [
    { label: '#', render: (a) => String(list.indexOf(a) + 1) },
    { label: 'Holder', render: (a) => a.holderName },
    { label: 'Account no.', render: (a) => String(a.accountNumber) },
    { label: 'Type', render: (a) => a.accountType },
    { label: 'Balance', num: true, render: (a) => formatINR(a.balance) },
  ], list, 'No accounts to sort yet.');
}
views.sorted = () => loadSorted();

views.about = async () => {
  try { const d = (await api.dashboard()).data; $('#aboutServer').textContent = 'Backend reachable. Accounts stored: ' + d.totalAccounts + '.'; }
  catch (e) { $('#aboutServer').textContent = 'Backend not reachable.'; }
};

// ---------- form wiring ----------
function wireForms() {
  $('#createForm').addEventListener('submit', (ev) => {
    ev.preventDefault(); const f = ev.target;
    const name = f.holderName.value.trim(), dep = f.initialDeposit.value.trim();
    if (!name) return formError(f, 'Please enter the account holder name');
    if (!reAmount.test(dep)) return formError(f, 'Enter the deposit as digits with up to 2 decimals, e.g. 1000.50');
    submitting(f, async () => {
      const res = await api.createAccount({ holderName: name, accountType: f.accountType.value, initialDeposit: dep });
      toast(res.message); f.reset(); location.hash = '#/details/' + res.data.accountNumber;
    });
  });

  for (const id of ['depositForm', 'withdrawForm']) {
    $('#' + id).addEventListener('submit', (ev) => {
      ev.preventDefault(); const f = ev.target; const kind = f.dataset.kind;
      const acc = f.accountNumber.value.trim(), amt = f.amount.value.trim();
      if (!reAccount.test(acc)) return formError(f, 'Account number must be digits only');
      if (!reAmount.test(amt)) return formError(f, 'Enter a positive amount with up to 2 decimals');
      submitting(f, async () => {
        const res = await api[kind]({ accountNumber: acc, amount: amt });
        toast(res.message + '. New balance ' + formatINR(res.data.account.balance));
        f.amount.value = '';
      });
    });
  }

  $('#transferForm').addEventListener('submit', (ev) => {
    ev.preventDefault(); const f = ev.target;
    const from = f.fromAccount.value.trim(), to = f.toAccount.value.trim(), amt = f.amount.value.trim();
    if (!reAccount.test(from) || !reAccount.test(to)) return formError(f, 'Account numbers must be digits only');
    if (from === to) return formError(f, 'Sender and receiver must be different accounts');
    if (!reAmount.test(amt)) return formError(f, 'Enter a positive amount with up to 2 decimals');
    submitting(f, async () => {
      const res = await api.transfer({ fromAccount: from, toAccount: to, amount: amt });
      toast(res.message); f.amount.value = '';
    });
  });

  $('#searchForm').addEventListener('submit', (ev) => { ev.preventDefault(); loadAccounts($('#searchBox').value.trim()).catch((e) => toast(e.message, true)); });
  $('#searchClear').addEventListener('click', () => { $('#searchBox').value = ''; loadAccounts('').catch((e) => toast(e.message, true)); });

  $('#detailsForm').addEventListener('submit', (ev) => {
    ev.preventDefault(); const n = $('#dNumber').value.trim();
    if (!reAccount.test(n)) { $('#detailsError').textContent = 'Account number must be digits only'; return; }
    location.hash = '#/details/' + n;
    showDetails(n);
  });
  $('#dClose').addEventListener('click', async () => {
    if (!currentAccount) return;
    const ok = await confirmDialog('Close account ' + currentAccount.accountNumber + '?',
      'The account of ' + currentAccount.holderName + ' will be permanently closed and its number will never be reused. ' +
      'Closing is only allowed when the balance is zero.', 'Close account');
    if (!ok) return;
    try { const res = await api.closeAccount(currentAccount.accountNumber); toast(res.message); location.hash = '#/accounts'; }
    catch (e) { toast(e.message, true); }
  });

  $('#historyForm').addEventListener('submit', (ev) => { ev.preventDefault(); loadHistory().catch((e) => { $('#historyError').textContent = e.message; }); });
  $('#historyClear').addEventListener('click', () => { $('#hAcc').value = ''; loadHistory().catch((e) => toast(e.message, true)); });

  $('#queueForm').addEventListener('submit', (ev) => {
    ev.preventDefault(); const f = ev.target; const name = $('#qName').value.trim();
    if (!name) return formError(f, 'Please enter the customer name');
    submitting(f, async () => {
      const res = await api.issueToken({ customerName: name, purpose: $('#qPurpose').value.trim() });
      toast(res.message); f.reset(); await loadQueue();
    });
  });
  $('#serveBtn').addEventListener('click', async () => {
    try { const res = await api.serveNext(); $('#servedMsg').textContent = res.message + ' (' + res.data.customerName + ')'; await loadQueue(); }
    catch (e) { $('#servedMsg').textContent = ''; toast(e.message, true); await loadQueue().catch(() => {}); }
  });

  $('#sortForm').addEventListener('submit', (ev) => { ev.preventDefault(); loadSorted().catch((e) => toast(e.message, true)); });
}

// ---------- router ----------
async function route() {
  const [, name = 'dashboard', param] = location.hash.split('/');
  const view = views[name] ? name : 'dashboard';
  document.querySelectorAll('[data-view]').forEach((s) => { s.hidden = s.dataset.view !== view; });
  document.querySelectorAll('[data-nav]').forEach((a) => a.classList.toggle('active', a.dataset.nav === view));
  $('#pageTitle').textContent = TITLES[view];
  document.title = TITLES[view] + ' - Bank Management System';
  $('#sidebar').classList.remove('open'); $('#menuBtn').setAttribute('aria-expanded', 'false');
  try { await views[view](param); setStatus(true); }
  catch (e) { if (e.code === 'NETWORK') setStatus(false); toast(e.message, true); }
}

function setStatus(online) {
  const s = $('#serverStatus');
  s.textContent = online ? 'Backend connected' : 'Backend offline';
  s.className = 'status ' + (online ? 'online' : 'offline');
  $('#offlineBanner').hidden = online;
}

document.addEventListener('DOMContentLoaded', () => {
  wireForms();
  $('#menuBtn').addEventListener('click', () => {
    const open = $('#sidebar').classList.toggle('open');
    $('#menuBtn').setAttribute('aria-expanded', String(open));
  });
  window.addEventListener('hashchange', route);
  route();
});
