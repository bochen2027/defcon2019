#ifndef INCLUDED_DROP_DOWN_MENU_H
#define INCLUDED_DROP_DOWN_MENU_H


#include <limits.h>

#include "core.h"


class DropDownOptionData
{
public:
	DropDownOptionData( const char *_word, int _value, bool _wordIsLanguagePhrase = false,
	                    const char *_renderLanguage = NULL );
	~DropDownOptionData();

	char	*m_word;
	int		m_value;
	bool	m_wordIsLanguagePhrase;
	char	*m_renderLanguage;
};


// ****************************************************************************
// Class DropDownMenu
// ****************************************************************************

class DropDownWindow : public InterfaceWindow
{
public:
    char m_parentName[256];
    static DropDownWindow *s_window;

public:
    DropDownWindow( char *_name, char *_title, bool _titleIsLanguagePhrase, char *_parentName );
    ~DropDownWindow();
    void Update();

    // There can be only one
    static DropDownWindow *CreateDropDownWindow( char *_name, char *_title, bool _titleIsLanguagePhrase, char *_parentName );
    static void RemoveDropDownWindow();
};


class DropDownMenu : public InterfaceButton
{
protected:
    LList   <DropDownOptionData *> m_options;
    int     m_currentOption;
    int     *m_int;
	bool	m_sortItems;
	int		m_nextValue;	// Used by AddOption if a value isn't passed in as an argument
	char    *m_orgcaption;
	bool    m_orgcaptionIsLanguagePhrase;
    
public:
    DropDownMenu(bool _sortItems = false);
    ~DropDownMenu();

    void            SetProperties       ( char *_name, int _x, int _y, int _w, int _h,
	                                      char *_caption = NULL, char *_tooltip = NULL, 
	                                      bool _captionIsLanguagePhrase = false, bool _tooltipIsLanguagePhrase = false );

    void            Empty               ();
    void            AddOption           ( char const *_option, int _value = INT_MIN, bool _optionIsLanguagePhrase = false,
                                          const char *_renderLanguage = NULL );
    int             GetSelectionValue   ();
	char const *	GetSelectionName	();
    virtual void    SelectOption        ( int _option );
	bool			SelectOption2		( char const *_option );
    
    void    CreateMenu();
    void    RemoveMenu();
    bool    IsMenuVisible();

    void    RegisterInt( int *_int );

	int		FindValue(int _value);	// Returns an index into m_options

    void Render     ( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp    ();
};


// ****************************************************************************
// Class DropDownMenuOption
// ****************************************************************************

class DropDownMenuOption : public TextButton
{
public:
    char    *m_parentWindowName;
    char    *m_parentMenuName;
//    int     m_menuIndex;
	int		m_value;			// An integer that the client specifies - often a value from an enum

public:
    DropDownMenuOption();
	~DropDownMenuOption();

    void SetParentMenu( EclWindow *_window, DropDownMenu *_menu, int _value );

    void Render ( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};


#endif
