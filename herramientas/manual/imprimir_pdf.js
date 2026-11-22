// Imprime un HTML a PDF A4 con Chromium (Playwright). Uso: node imprimir_pdf.js entrada.html salida.pdf "Titulo del encabezado"
const { chromium } = require('playwright');
const path = require('path');
(async () => {
  const [, , entrada, salida, titulo] = process.argv;
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('file://' + path.resolve(entrada), { waitUntil: 'load' });
  await page.evaluate(() => document.fonts.ready);
  await page.pdf({
    path: salida, format: 'A4', printBackground: true, preferCSSPageSize: true,
    displayHeaderFooter: true,
    headerTemplate: `<div style="font-family:Inter,Arial,sans-serif;font-size:7.5pt;color:#6b7b8b;width:100%;padding:0 17mm;display:flex;justify-content:space-between"><span>${titulo || ''}</span><span></span></div>`,
    footerTemplate: `<div style="font-family:Inter,Arial,sans-serif;font-size:7.5pt;color:#6b7b8b;width:100%;padding:0 17mm;display:flex;justify-content:space-between"><span>ARQUIS</span><span>Página <span class="pageNumber"></span> de <span class="totalPages"></span></span></div>`,
    margin: { top: '20mm', bottom: '20mm', left: '17mm', right: '17mm' },
  });
  await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
