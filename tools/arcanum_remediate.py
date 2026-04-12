#!/usr/bin/env python3
"""Arcanum wiki remediation: add title+tags frontmatter, split files >80 lines."""

import os, re, sys
from pathlib import Path

ARCANUM = Path(r"C:/Users/atayl/VoxCore/doc/arcanum")
MAX_LINES = 80
SKIP_DIRS = {"source"}


def parse_file(text):
    """Returns (fm_lines_list, fm_dict, body_str, had_fm)."""
    lines = text.split('\n')
    if not lines or lines[0].strip() != '---':
        return [], {}, text, False
    end = None
    for i in range(1, len(lines)):
        if lines[i].strip() == '---':
            end = i
            break
    if end is None:
        return [], {}, text, False

    fm_raw = lines[1:end]
    fm = {}
    for line in fm_raw:
        m = re.match(r'^(\w+):\s*(.+)$', line.strip())
        if m:
            val = m.group(2).strip().strip('"').strip("'")
            fm[m.group(1)] = val
    body = '\n'.join(lines[end + 1:]).strip()
    return fm_raw, fm, body, True


def get_title(fm, body, filename):
    if fm.get('title'):
        return fm['title']
    for line in body.split('\n'):
        if line.startswith('# ') and not line.startswith('## '):
            return line[2:].strip()
    return filename.replace('.md', '').replace('_', ' ').title()


def get_tags(fm, desc, folder):
    if fm.get('tags'):
        return fm['tags']
    tags = []
    if folder:
        tags.append(folder)
    if desc:
        parts = re.split(r'[—–]', desc, maxsplit=1)
        kw_part = parts[-1] if len(parts) > 1 else desc
        for term in re.split(r'[,;]', kw_part):
            t = term.strip().lower()
            t = re.sub(r'[^a-z0-9\s-]', '', t).strip()
            if t and 2 < len(t) < 25:
                tag = '-'.join(t.split()[:2])
                if tag not in tags:
                    tags.append(tag)
            if len(tags) >= 8:
                break
    return ', '.join(tags) if tags else 'general'


def insert_fm_fields(text, title, tags):
    """Insert title and tags into existing frontmatter, preserving description."""
    lines = text.split('\n')
    if lines[0].strip() != '---':
        return f'---\ntitle: "{title}"\ntags: [{tags}]\n---\n\n{text}'
    end = None
    for i in range(1, len(lines)):
        if lines[i].strip() == '---':
            end = i
            break
    if end is None:
        return text

    has_title = any(l.strip().startswith('title:') for l in lines[1:end])
    has_tags = any(l.strip().startswith('tags:') for l in lines[1:end])

    insert = []
    if not has_title:
        insert.append(f'title: "{title}"')
    if not has_tags:
        insert.append(f'tags: [{tags}]')
    if not insert:
        return text
    new_lines = lines[:end] + insert + lines[end:]
    return '\n'.join(new_lines)


def split_at_headers(body, header_re=r'^## '):
    """Split body into sections at header boundaries."""
    sections = []
    current = []
    for line in body.split('\n'):
        if re.match(header_re, line) and current:
            sections.append(current)
            current = [line]
        else:
            current.append(line)
    if current:
        sections.append(current)
    return sections


def split_large_section(section_lines, max_lines):
    """Split an oversized section at ### headers, then blank lines."""
    # Try ### headers
    subsections = []
    current = []
    for line in section_lines:
        if re.match(r'^### ', line) and current:
            subsections.append(current)
            current = [line]
        else:
            current.append(line)
    if current:
        subsections.append(current)

    if len(subsections) > 1:
        return group_chunks(subsections, max_lines)

    # Try blank-line splits
    paragraphs = []
    current = []
    for line in section_lines:
        if line.strip() == '' and current:
            current.append(line)
            paragraphs.append(current)
            current = []
        else:
            current.append(line)
    if current:
        paragraphs.append(current)

    if len(paragraphs) > 1:
        return group_chunks(paragraphs, max_lines)

    # Can't split further — return as-is
    return [section_lines]


def group_chunks(chunks, max_lines):
    """Group small chunks into parts that fit within max_lines."""
    parts = []
    current = []
    count = 0
    for chunk in chunks:
        cl = len(chunk)
        if count + cl > max_lines and current:
            parts.append(current)
            current = list(chunk)
            count = cl
        else:
            current.extend(chunk)
            count += cl
    if current:
        parts.append(current)
    return parts


def split_body(body, max_body_lines):
    """Split body into parts, each <=max_body_lines. Returns list of line-lists."""
    sections = split_at_headers(body)

    # First pass: group sections into parts
    raw_parts = group_chunks(sections, max_body_lines)

    # Second pass: split any oversized parts
    final_parts = []
    for part in raw_parts:
        if len(part) <= max_body_lines:
            final_parts.append(part)
        else:
            sub = split_large_section(part, max_body_lines)
            final_parts.extend(sub)

    return final_parts


def write_parts(parent, stem, parts, title, desc, tags):
    """Write split parts to disk."""
    n = len(parts)
    created = []
    for i, part_lines in enumerate(parts, 1):
        pt_title = f"{title} (Part {i}/{n})"
        t_esc = pt_title.replace('"', '\\"')
        d_esc = desc.replace('"', '\\"') if desc else ''

        fm = ['---']
        fm.append(f'title: "{t_esc}"')
        if d_esc:
            fm.append(f'description: "{d_esc}"')
        fm.append(f'tags: [{tags}]')
        fm.append(f'part: {i}')
        fm.append(f'parts: {n}')
        fm.append('---')

        # Navigation footer
        nav_links = []
        for j in range(1, n + 1):
            if j == i:
                nav_links.append(f'**Part {j}**')
            else:
                nav_links.append(f'[Part {j}]({stem}_pt{j}.md)')
        nav = ' | '.join(nav_links)

        content = '\n'.join(fm) + '\n\n' + '\n'.join(part_lines).strip() + '\n\n---\n' + nav + '\n'

        pt_path = parent / f"{stem}_pt{i}.md"
        pt_path.write_text(content, encoding='utf-8')
        created.append(pt_path.name)

    return created


def process_all():
    fixed = 0
    split_count = 0
    parts_total = 0
    errors = 0
    still_over = 0

    for root, dirs, files in os.walk(ARCANUM):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
        rel = Path(root).relative_to(ARCANUM)
        folder = str(rel).split('/')[0] if str(rel) != '.' else ''
        if '\\' in folder:
            folder = folder.split('\\')[0]

        for fname in sorted(files):
            if not fname.endswith('.md'):
                continue
            fpath = Path(root) / fname
            try:
                text = fpath.read_text(encoding='utf-8')
            except Exception as e:
                print(f"ERROR read {fpath.relative_to(ARCANUM)}: {e}")
                errors += 1
                continue

            fm_raw, fm, body, had_fm = parse_file(text)
            title = get_title(fm, body, fname)
            desc = fm.get('description', '')
            tags = get_tags(fm, desc, folder)

            # Fix frontmatter first
            fixed_text = insert_fm_fields(text, title, tags)
            total_lines = len(fixed_text.split('\n'))

            if total_lines <= MAX_LINES:
                fpath.write_text(fixed_text, encoding='utf-8')
                fixed += 1
                continue

            # Need to split — budget ~70 body lines per part (fm=7, nav=3)
            max_body = MAX_LINES - 10
            body_parts = split_body(body, max_body)

            if len(body_parts) <= 1:
                # Can't split — just fix frontmatter, flag as over
                fpath.write_text(fixed_text, encoding='utf-8')
                fixed += 1
                still_over += 1
                lc = len(fixed_text.split('\n'))
                print(f"OVER  {fpath.relative_to(ARCANUM)} ({lc} lines, could not split)")
                continue

            created = write_parts(fpath.parent, fpath.stem, body_parts, title, desc, tags)
            fpath.unlink()
            split_count += 1
            parts_total += len(created)
            print(f"SPLIT {fpath.relative_to(ARCANUM)} -> {len(created)} parts: {', '.join(created)}")

    print(f"\n{'='*50}")
    print(f"Frontmatter fixed (no split needed): {fixed}")
    print(f"Files split: {split_count} -> {parts_total} parts")
    print(f"Still over limit (unsplittable): {still_over}")
    print(f"Errors: {errors}")
    print(f"{'='*50}")


if __name__ == '__main__':
    process_all()
