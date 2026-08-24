/////////////////////////////////////////////////////////////////////////////
// Minimal wxCheckListBox for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHECKLISTBOX

#ifndef WX_PRECOMP
    #include "wx/checklst.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxCheckListBox, wxListBox);

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            int n, const wxString choices[],
                            long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !wxListBox::Create(parent, id, pos, size, 0, nullptr, style, validator, name) )
        return false;

    for ( int i = 0; i < n; ++i )
        Append(choices[i]);

    return true;
}

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            const wxArrayString& choices,
                            long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !wxListBox::Create(parent, id, pos, size, 0, nullptr, style, validator, name) )
        return false;

    Append(choices);
    return true;
}

bool wxCheckListBox::IsChecked(unsigned int item) const
{
    wxCHECK_MSG( item < m_checks.size(), false, "invalid index" );
    return m_checks[item] != 0;
}

void wxCheckListBox::Check(unsigned int item, bool check)
{
    wxCHECK_RET( item < m_checks.size(), "invalid index" );
    m_checks[item] = check ? 1 : 0;
}

int wxCheckListBox::DoInsertItems(const wxArrayStringsAdapter& items,
                                  unsigned int pos,
                                  void **clientData,
                                  wxClientDataType type)
{
    const int n = wxListBox::DoInsertItems(items, pos, clientData, type);
    const unsigned int count = items.GetCount();
    for ( unsigned int i = 0; i < count; ++i )
        m_checks.Insert(0, pos + i);
    return n;
}

void wxCheckListBox::DoClear()
{
    wxListBox::DoClear();
    m_checks.Clear();
}

void wxCheckListBox::DoDeleteOneItem(unsigned int pos)
{
    wxListBox::DoDeleteOneItem(pos);
    m_checks.RemoveAt(pos);
}

#endif // wxUSE_CHECKLISTBOX
