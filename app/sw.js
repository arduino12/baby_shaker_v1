// Network-first app shell. Every request revalidates with the server
// (cache: 'no-cache'), so a page, its CSS and its JS always come from the same
// release - GitHub Pages lets browsers cache files for 10 minutes, which once
// mixed an old app.js with a new index.html. The cache is only the offline
// fallback (e.g. on a walk with no signal).
const CACHE = 'baby-shaker-1.5.0';
const SHELL = ['./', 'index.html', 'app.js', 'style.css', 'manifest.json', 'icon.svg'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(SHELL.map(u => new Request(u, { cache: 'no-cache' })))));
  self.skipWaiting();
});

self.addEventListener('activate', e => e.waitUntil(
  caches.keys().then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
    .then(() => self.clients.claim())));

self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET' || !e.request.url.startsWith(self.location.origin)) return;
  if (e.request.url.includes('/fw/')) return;   // firmware: always straight from the network
  e.respondWith(
    fetch(e.request, { cache: 'no-cache' })
      .then(r => {
        const copy = r.clone();
        caches.open(CACHE).then(c => c.put(e.request, copy));
        return r;
      })
      .catch(() => caches.match(e.request, { ignoreSearch: true }))
  );
});
