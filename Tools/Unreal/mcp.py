"""Calls the Unreal editor's built-in MCP server (the experimental ModelContextProtocol plugin).

The editor serves MCP over HTTP at http://localhost:8000/mcp once the plugin is enabled and
bAutoStartServer is set (per user, in Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini).
Claude Code picks it up from .mcp.json; this is the same thing from a shell, for scripts and for
sessions that started before the editor did.

    python Tools/Unreal/mcp.py toolsets
    python Tools/Unreal/mcp.py describe <toolset>
    python Tools/Unreal/mcp.py call <toolset> <tool> '{"arg": 1}'
"""
import json
import sys
import urllib.request

URL = "http://localhost:8000/mcp"


def post(body, session=None):
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if session:
        headers["Mcp-Session-Id"] = session
    request = urllib.request.Request(URL, data=json.dumps(body).encode(), headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=600) as response:
        text = response.read().decode("utf-8", "replace")
        session_id = response.headers.get("Mcp-Session-Id", session)
    # Streamable HTTP may answer as an event stream; the JSON is on the data: line.
    if text.lstrip().startswith("event:") or "\ndata:" in text:
        text = "\n".join(line[5:] for line in text.splitlines() if line.startswith("data:"))
    return (json.loads(text) if text.strip() else None), session_id


def open_session():
    _, session = post({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                       "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                  "clientInfo": {"name": "dtg-tools", "version": "1"}}})
    post({"jsonrpc": "2.0", "method": "notifications/initialized"}, session)
    return session


def call(session, name, arguments):
    reply, _ = post({"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                     "params": {"name": name, "arguments": arguments}}, session)
    if reply is None:
        return ""
    if "error" in reply:
        return "ERROR: " + json.dumps(reply["error"])
    return "\n".join(part.get("text", json.dumps(part)) for part in reply["result"].get("content", []))


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    session = open_session()
    if argv[0] == "toolsets":
        print(call(session, "list_toolsets", {}))
    elif argv[0] == "describe" and len(argv) == 2:
        print(call(session, "describe_toolset", {"toolset_name": argv[1]}))
    elif argv[0] == "call" and len(argv) in (3, 4):
        arguments = json.loads(argv[3]) if len(argv) == 4 else {}
        print(call(session, "call_tool", {"toolset_name": argv[1], "tool_name": argv[2], "arguments": arguments}))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
