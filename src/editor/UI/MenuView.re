open Revery;
open Revery.UI;
open Revery.UI.Components;
open Oni_Core;
open Oni_Model;

let component = React.component("Menu");

let menuWidth = 400;
let menuHeight = 350;

let containerStyles = (theme: Theme.t) =>
  Style.[
    backgroundColor(theme.colors.editorMenuBackground),
    color(theme.colors.editorMenuForeground),
    width(menuWidth),
    height(menuHeight),
    boxShadow(
      ~xOffset=-15.,
      ~yOffset=5.,
      ~blurRadius=30.,
      ~spreadRadius=5.,
      ~color=Color.rgba(0., 0., 0., 0.2),
    ),
  ];

let menuItemStyle = Style.[fontSize(14), width(menuWidth - 50)];

let inputStyles = font =>
  Style.[
    border(~width=2, ~color=Color.rgba(0., 0., 0., 0.1)),
    backgroundColor(Color.rgba(0., 0., 0., 0.3)),
    width(menuWidth - 10),
    color(Colors.white),
    fontFamily(font),
  ];

let handleChange = (event: Input.changeEvent) =>
  GlobalContext.current().dispatch(MenuSearch(event.value));

let handleKeyDown = (event: NodeEvents.keyEventParams) =>
  switch (event) {
  | {key: Revery.Key.KEY_ESCAPE, _} =>
    GlobalContext.current().dispatch(SetInputControlMode(MenuFocus))
  | _ => ()
  };

let loseFocusOnClose = isOpen =>
  /**
   TODO: revery-ui/revery#412 if the menu is hidden abruptly the element is not automatically unfocused
   as revery is unaware the element is no longer in focus
 */
  (
    switch (Focus.focused, isOpen) {
    | ({contents: Some(_)}, false) => Focus.loseFocus()
    | (_, _) => ()
    }
  );

let onClick = () => {
  GlobalContext.current().dispatch(MenuSelect);
  GlobalContext.current().dispatch(SetInputControlMode(EditorTextFocus));
};

let onMouseOver = pos => GlobalContext.current().dispatch(MenuPosition(pos));

type fontT = Types.UiFont.t;

let getLabel = (command: Actions.menuCommand) => {
  switch (command.category) {
  | Some(v) => v ++ ": " ++ command.name
  | None => command.name
  };
};

let createElement =
    (~children as _, ~font: fontT, ~menu: Menu.t, ~theme: Theme.t, ()) =>
  component(hooks => {
    let hooks =
      React.Hooks.effect(
        Always,
        () => {
          loseFocusOnClose(menu.isOpen);
          None;
        },
        hooks,
      );

    React.(
      hooks,
      menu.isOpen
        ? <View style={containerStyles(theme)}>
            <View style=Style.[width(menuWidth), padding(5)]>
              <Input
                autofocus=true
                placeholder="type here to search the menu"
                cursorColor=Colors.white
                style={inputStyles(font.fontFile)}
                onChange=handleChange
                onKeyDown=handleKeyDown
              />
            </View>
            <View>
              <FlatList
                rowHeight=40
                height={menuHeight - 50}
                width=menuWidth
                count={List.length(menu.filteredCommands)}
                render={index => {
                  let cmd = List.nth(menu.filteredCommands, index);
                  <MenuItem
                    onClick
                    theme
                    style=menuItemStyle
                    label={getLabel(cmd)}
                    icon={cmd.icon}
                    onMouseOver={_ => onMouseOver(index)}
                    selected={index == menu.selectedItem}
                  />;
                }}
              />
            </View>
          </View>
        : React.listToElement([]),
    );
  });
