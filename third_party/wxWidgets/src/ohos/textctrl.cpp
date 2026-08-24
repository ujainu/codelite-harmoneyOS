/////////////////////////////////////////////////////////////////////////////
// Minimal wxTextCtrl for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TEXTCTRL

#ifndef WX_PRECOMP
    #include "wx/textctrl.h"
#endif

#include "wx/arrstr.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxTextCtrl, wxControl);

bool wxTextCtrl::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxString& value,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    DoSetValue(value, 0);
    return true;
}

int wxTextCtrl::GetLineLength(long lineNo) const
{
    return GetLineText(lineNo).length();
}

wxString wxTextCtrl::GetLineText(long lineNo) const
{
    wxArrayString lines = wxSplit(m_value, '\n', '\0');
    if ( lineNo < 0 || lineNo >= (long)lines.size() )
        return wxEmptyString;
    return lines[(size_t)lineNo];
}

int wxTextCtrl::GetNumberOfLines() const
{
    if ( m_value.empty() )
        return 1;
    return (int)wxSplit(m_value, '\n', '\0').size();
}

bool wxTextCtrl::IsModified() const { return m_modified; }
void wxTextCtrl::MarkDirty() { m_modified = true; }
void wxTextCtrl::DiscardEdits() { m_modified = false; }

long wxTextCtrl::XYToPosition(long x, long y) const
{
    long pos = 0;
    for ( long line = 0; line < y; ++line )
        pos += GetLineLength(line) + 1;
    return pos + x;
}

bool wxTextCtrl::PositionToXY(long pos, long *x, long *y) const
{
    if ( pos < 0 || pos > GetLastPosition() )
        return false;

    long line = 0;
    long col = pos;
    const wxArrayString lines = wxSplit(m_value, '\n', '\0');
    for ( size_t i = 0; i < lines.size(); ++i )
    {
        const long len = (long)lines[i].length();
        if ( col <= len )
        {
            if ( x ) *x = col;
            if ( y ) *y = line;
            return true;
        }
        col -= len + 1;
        ++line;
    }
    if ( x ) *x = 0;
    if ( y ) *y = line;
    return true;
}

void wxTextCtrl::ShowPosition(long WXUNUSED(pos)) {}

void wxTextCtrl::SetInsertionPoint(long pos)
{
    m_insertionPoint = pos < 0 ? GetLastPosition() : pos;
}

long wxTextCtrl::GetInsertionPoint() const { return m_insertionPoint; }
long wxTextCtrl::GetLastPosition() const { return (long)m_value.length(); }

void wxTextCtrl::SetSelection(long from, long to)
{
    m_selFrom = from;
    m_selTo = to;
}

void wxTextCtrl::GetSelection(long *from, long *to) const
{
    if ( from ) *from = m_selFrom;
    if ( to ) *to = m_selTo;
}

void wxTextCtrl::Copy() {}
void wxTextCtrl::Cut() {}
void wxTextCtrl::Paste() {}
void wxTextCtrl::Undo() {}
void wxTextCtrl::Redo() {}
bool wxTextCtrl::CanUndo() const { return false; }
bool wxTextCtrl::CanRedo() const { return false; }

void wxTextCtrl::Remove(long from, long to)
{
    if ( to < 0 || to > GetLastPosition() )
        to = GetLastPosition();
    if ( from < 0 )
        from = 0;
    if ( from >= to )
        return;
    m_value.erase((size_t)from, (size_t)(to - from));
    m_modified = true;
}

bool wxTextCtrl::IsEditable() const { return m_editable; }
void wxTextCtrl::SetEditable(bool editable) { m_editable = editable; }

wxString wxTextCtrl::DoGetValue() const { return m_value; }

void wxTextCtrl::DoSetValue(const wxString& value, int flags)
{
    m_value = value;
    m_insertionPoint = (long)m_value.length();
    m_selFrom = m_selTo = m_insertionPoint;
    m_modified = false;
    if ( flags & SetValue_SendEvent )
        SendTextUpdatedEventIfAllowed();
}

void wxTextCtrl::WriteText(const wxString& text)
{
    if ( m_selFrom != m_selTo )
        Remove(m_selFrom, m_selTo);
    m_value.insert((size_t)m_insertionPoint, text);
    m_insertionPoint += (long)text.length();
    m_modified = true;
}

wxSize wxTextCtrl::DoGetBestSize() const
{
    return wxSize(100, 24);
}

#endif // wxUSE_TEXTCTRL
