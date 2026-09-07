import os
import sys
import json
import argparse
from datetime import datetime, timezone
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

SCOPES = ["https://www.googleapis.com/auth/documents"]

SCRIPT_DIR = r"D:\ai agent\agent-system"
CREDENTIALS_FILE = os.path.join(SCRIPT_DIR, "credentials.json")
TOKEN_FILE = os.path.join(SCRIPT_DIR, "token.json")

def save_credentials(creds):
    temporary = TOKEN_FILE + ".tmp"
    with open(temporary, "w", encoding="utf-8") as token:
        token.write(creds.to_json())
    os.replace(temporary, TOKEN_FILE)


def authenticate():
    flow = InstalledAppFlow.from_client_secrets_file(CREDENTIALS_FILE, SCOPES)
    creds = flow.run_local_server(
        host="127.0.0.1", port=0, open_browser=False, timeout_seconds=300,
        authorization_prompt_message="AUTH_URL:{url}",
        success_message="Google Docs connected. You can close this window.",
        access_type="offline", prompt="consent",
    )
    save_credentials(creds)
    print("Google Docs authentication saved. No code needs to be pasted into chat.")


def get_credentials():
    if not os.path.exists(TOKEN_FILE):
        raise RuntimeError("Google Docs is not connected. Run gdocs.cmd auth once; open AUTH_URL in your browser.")
    creds = Credentials.from_authorized_user_file(TOKEN_FILE, SCOPES)
    if not creds.valid:
        if not creds.refresh_token:
            raise RuntimeError("Google Docs needs authorization again: gdocs.cmd auth")
        creds.refresh(Request())
        save_credentials(creds)
    return creds


def extract_text_from_elements(elements):
    text_parts = []
    for element in elements:
        if "paragraph" in element:
            paragraph = element["paragraph"]
            for el in paragraph.get("elements", []):
                text_run = el.get("textRun")
                if text_run:
                    text_parts.append(text_run.get("content", ""))
        elif "table" in element:
            table = element["table"]
            for row in table.get("tableRows", []):
                for cell in row.get("tableCells", []):
                    text_parts.append(extract_text_from_elements(cell.get("content", [])))
        elif "tableOfContents" in element:
            toc = element["tableOfContents"]
            text_parts.append(extract_text_from_elements(toc.get("content", [])))
    return "".join(text_parts)

def get_tab_content(doc, tab_id=None):
    tabs = doc.get("tabs", [])
    if tabs:
        if tab_id:
            def search_tab(tab_list):
                for t in tab_list:
                    props = t.get("tabProperties", {})
                    if props.get("tabId") == tab_id:
                        return t
                    child_tabs = t.get("childTabs", [])
                    if child_tabs:
                        res = search_tab(child_tabs)
                        if res:
                            return res
                return None
            target_tab = search_tab(tabs)
            if target_tab:
                doc_tab = target_tab.get("documentTab", {})
                body = doc_tab.get("body", {})
                return extract_text_from_elements(body.get("content", []))
            else:
                raise ValueError(f"Requested tab not found: {tab_id}")
        first_tab = tabs[0]
        doc_tab = first_tab.get("documentTab", {})
        body = doc_tab.get("body", {})
        return extract_text_from_elements(body.get("content", []))
    
    body = doc.get("body", {})
    return extract_text_from_elements(body.get("content", []))

def read_text(document_id, tab_id=None):
    creds = get_credentials()
    service = build("docs", "v1", credentials=creds)
    doc = service.documents().get(documentId=document_id, includeTabsContent=True).execute()
    revision_id = doc.get("revisionId", "")
    text = get_tab_content(doc, None if tab_id == "all" else tab_id)
    return text, revision_id

def cache_tab(document_id, tab_id, scope_name, target_dir):
    text, revision_id = read_text(document_id, tab_id)
    os.makedirs(target_dir, exist_ok=True)
    tab_suffix = tab_id if tab_id else "t.0"
    clean_scope = "".join(c for c in scope_name if c.isalnum() or c in ("-", "_")).strip() or "content"
    filename = f"{document_id}__{tab_suffix}__{clean_scope}.md"
    file_path = os.path.join(target_dir, filename)

    now_iso = datetime.now(timezone.utc).isoformat()
    header = [
        f"source_url: https://docs.google.com/document/d/{document_id}/edit",
        f"tab_id: {tab_id or 't.0'}",
        f"scope: {scope_name}",
        f"fetched_at: {now_iso}",
        f"revision_id: {revision_id}",
        "",
        ""
    ]
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("\n".join(header) + text)
    print(f"Cached successfully to: {file_path}")
    return file_path

def replace_document_text(document_id, new_content):
    creds = get_credentials()
    service = build("docs", "v1", credentials=creds)
    doc = service.documents().get(documentId=document_id).execute()
    body = doc.get("body", {})
    content = body.get("content", [])
    
    end_index = 1
    if content:
        end_index = content[-1].get("endIndex", 1)
    
    requests = []
    # If document has existing content beyond the required terminal newline at index 1
    if end_index > 2:
        requests.append({
            "deleteContentRange": {
                "range": {
                    "startIndex": 1,
                    "endIndex": end_index - 1
                }
            }
        })
    requests.append({
        "insertText": {
            "location": {
                "index": 1
            },
            "text": new_content
        }
    })
    
    result = service.documents().batchUpdate(
        documentId=document_id,
        body={"requests": requests, "writeControl": {"requiredRevisionId": doc["revisionId"]}}
    ).execute()
    print(f"Successfully updated document {document_id}")
    return result

def main():
    parser = argparse.ArgumentParser(description="Google Docs CLI Tool for Agents")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("auth", help="One-time Google Docs browser authorization")

    read_parser = subparsers.add_parser("read-text", help="Read text content of a Google Doc or specific tab")
    read_parser.add_argument("document_id", help="The Google Doc Document ID")
    read_parser.add_argument("--tab-id", help="Optional Tab ID (e.g. t.zg6q672r2g15)", default=None)

    cache_parser = subparsers.add_parser("cache-tab", help="Fetch and save tab content to a markdown cache file")
    cache_parser.add_argument("document_id", help="The Google Doc Document ID")
    cache_parser.add_argument("tab_id", help="The Tab ID (or t.0 if main)", default="t.0")
    cache_parser.add_argument("scope_name", help="Scope description or scope tag")
    cache_parser.add_argument("--target-dir", help="Directory to save the cache", default=None)

    update_parser = subparsers.add_parser("update-text", help="Replace document text with content from file or stdin")
    update_parser.add_argument("document_id", help="The Google Doc Document ID")
    update_parser.add_argument("file_path", help="Path to text file to write into document")

    args = parser.parse_args()

    if args.command == "auth":
        authenticate()
    elif args.command == "read-text":
        text, revision_id = read_text(args.document_id, args.tab_id)
        sys.stdout.buffer.write(text.encode("utf-8"))
    elif args.command == "cache-tab":
        target_dir = args.target_dir
        if not target_dir:
            default_path = r"D:\ai agent\projects\mythicmobs\.agent-work\docs-cache"
            target_dir = default_path if os.path.exists(r"D:\ai agent\projects\mythicmobs") else os.path.join(SCRIPT_DIR, "docs-cache")
        cache_tab(args.document_id, args.tab_id, args.scope_name, target_dir)
    elif args.command == "update-text":
        with open(args.file_path, "r", encoding="utf-8") as f:
            content = f.read()
        replace_document_text(args.document_id, content)

if __name__ == "__main__":
    main()
