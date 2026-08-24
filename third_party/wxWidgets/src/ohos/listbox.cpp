/////////////////////////////////////////////////////////////////////////////
// Minimal wxListBox for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_LISTBOX

#ifndef WX_PRECOMP
    #include "wx/listbox.h"
#endif

#include "wx/arrstr.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxListBox, wxControl);

bool wxListBox::Create(wxWindow *parent, wxWindowID id,
                       const wxPoint& pos, const wxSize& size,
                       int n, const wxString choices[],
                       long style,
                       const wxValidator& validator,
                       const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    for ( int i = 0; i < n; ++i )
        Append(choices[i]);

    return true;
}

bool wxListBox::Create(wxWindow *parent, wxWindowID id,
                       const wxPoint& pos, const wxSize& size,
                       const wxArrayString& choices,
                       long style,
                       const wxValidator& validator,
                       const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    Append(choices);
    return true;
}

bool wxListBox::IsSelected(int n) const
{
    return m_selections.Index(n) != wxNOT_FOUND;
}

int wxListBox::GetSelections(wxArrayInt& aSelections) const
{
    aSelections = m_selections;
    return (int)m_selections.size();
}

unsigned int wxListBox::GetCount() const
{
    return (unsigned int)m_strings.size();
}

wxString wxListBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( n < GetCount(), wxEmptyString, "invalid index" );
    return m_strings[n];
}

void wxListBox::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET( n < GetCount(), "invalid index" );
    m_strings[n] = s;
}

int wxListBox::GetSelection() const
{
    return m_selections.empty() ? wxNOT_FOUND : m_selections[0];
}

void wxListBox::DoSetFirstItem(int WXUNUSED(n)) {}

void wxListBox::DoSetSelection(int n, bool select)
{
    if ( n == wxNOT_FOUND )
    {
        m_selections.Clear();
        return;
    }

    if ( select )
    {
        if ( !HasMultipleSelection() )
            m_selections.Clear();
        if ( m_selections.Index(n) == wxNOT_FOUND )
            m_selections.Add(n);
    }
    else
    {
        const int idx = m_selections.Index(n);
        if ( idx != wxNOT_FOUND )
            m_selections.RemoveAt(idx);
    }
}

int wxListBox::DoInsertItems(const wxArrayStringsAdapter& items,
                             unsigned int pos,
                             void **clientData,
                             wxClientDataType type)
{
    const unsigned int count = items.GetCount();
    for ( unsigned int i = 0; i < count; ++i )
    {
        m_strings.Insert(items[i], pos + i);
        m_clientData.Insert(nullptr, pos + i);
        AssignNewItemClientData(pos + i, clientData, i, type);
    }
    return (int)(pos + count - 1);
}

void wxListBox::DoSetItemClientData(unsigned int n, void *clientData)
{
    m_clientData[n] = clientData;
}

void *wxListBox::DoGetItemClientData(unsigned int n) const
{
    return m_clientData[n];
}

void wxListBox::DoClear()
{
    m_strings.Clear();
    m_clientData.Clear();
    m_selections.Clear();
}

void wxListBox::DoDeleteOneItem(unsigned int pos)
{
    m_strings.RemoveAt(pos);
    m_clientData.RemoveAt(pos);

    for ( int i = (int)m_selections.size() - 1; i >= 0; --i )
    {
        if ( m_selections[(size_t)i] == (int)pos )
            m_selections.RemoveAt((size_t)i);
        else if ( m_selections[(size_t)i] > (int)pos )
            --m_selections[(size_t)i];
    }
}

#endif // wxUSE_LISTBOX
