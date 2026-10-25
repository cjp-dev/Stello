import fs from 'fs';
import path from 'path';

const docsBrainDir = path.resolve('docs/brain');
const plansDir = path.resolve('Plans');

const filesToAudit = [
  ...fs.readdirSync(docsBrainDir).filter(f => f.endsWith('.md')).map(f => path.join(docsBrainDir, f)),
  path.join(plansDir, 'general-game-gui-specification.md'),
  path.join(plansDir, 'Stello porting documentation.md')
];

let totalBlocks = 0;
let errors = [];
let warnings = [];

const blockCatalog = [];

for (const filePath of filesToAudit) {
  if (!fs.existsSync(filePath)) {
    warnings.push(`File missing: ${filePath}`);
    continue;
  }
  const relPath = path.relative(process.cwd(), filePath);
  const content = fs.readFileSync(filePath, 'utf8');
  const lines = content.split(/\r?\n/);

  let inBlock = false;
  let blockStart = 0;
  let blockLang = '';
  let blockContent = [];

  for (let i = 0; i < lines.length; i++) {
    const lineNum = i + 1;
    const line = lines[i];

    if (line.startsWith('```')) {
      if (!inBlock) {
        inBlock = true;
        blockStart = lineNum;
        blockLang = line.slice(3).trim();
        blockContent = [];
        totalBlocks++;

        if (blockLang === '') {
          errors.push(`${relPath}:${lineNum} - Bare fenced code block without language tag!`);
        }
      } else {
        inBlock = false;
        blockCatalog.push({
          file: relPath,
          start: blockStart,
          end: lineNum,
          lang: blockLang,
          linesCount: lineNum - blockStart - 1,
          content: blockContent.join('\n')
        });
      }
    } else if (inBlock) {
      blockContent.push(line);
      // Check for double numbering inside text blocks
      const insideDoubleMatch = line.match(/^\s*\d+\.\s+\d+\.\s+/);
      if (insideDoubleMatch) {
        errors.push(`${relPath}:${lineNum} - Double-numbered list artifact inside block: "${line}"`);
      }
    } else {
      // Check prose lines
      // Check for double numbering like "1. 1. "
      const doubleNumMatch = line.match(/^\s*\d+\.\s+\d+\.\s+/);
      if (doubleNumMatch) {
        errors.push(`${relPath}:${lineNum} - Double-numbered list artifact: "${line}"`);
      }
    }
  }
}

console.log(`Audited ${filesToAudit.length} files.`);
console.log(`Total fenced code blocks cataloged: ${totalBlocks}`);

console.log('\n--- Fenced Code Blocks by Language ---');
const langCounts = {};
for (const b of blockCatalog) {
  langCounts[b.lang || '(bare)'] = (langCounts[b.lang || '(bare)'] || 0) + 1;
}
console.table(langCounts);

if (errors.length > 0) {
  console.log('\n--- Discrepancies & Formatting Errors Found ---');
  for (const err of errors) {
    console.log(`[ERROR] ${err}`);
  }
} else {
  console.log('\n[PASS] No syntax or formatting errors found.');
}

if (warnings.length > 0) {
  console.log('\n--- Warnings ---');
  for (const w of warnings) {
    console.log(`[WARN] ${w}`);
  }
}
