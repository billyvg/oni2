/*
 * ExtensionClientStoreConnector.re
 *
 * This connects the extension client to the store:
 * - Converts extension host notifications into ACTIONS
 * - Calls appropriate APIs on extension host based on ACTIONS
 */

module Core = Oni_Core;
module Model = Oni_Model;

open Oni_Extensions;
module Extensions = Oni_Extensions;
module Protocol = Extensions.ExtHostProtocol;

let start = (extensions, setup: Core.Setup.t) => {
  let (stream, dispatch) = Isolinear.Stream.create();

  let onExtHostClosed = () => print_endline("ext host closed");

  let extensionInfo =
    extensions
    |> List.map(ext =>
         Extensions.ExtHostInitData.ExtensionInfo.ofScannedExtension(ext)
       );

  let onStatusBarSetEntry = ((id, text, alignment, priority)) => {
    dispatch(
      Model.Actions.StatusBarAddItem(
        Model.StatusBarModel.Item.create(
          ~id,
          ~text,
          ~alignment=Model.StatusBarModel.Alignment.ofInt(alignment),
          ~priority,
          (),
        ),
      ),
    );
  };

  let initData = ExtHostInitData.create(~extensions=extensionInfo, ());
  let extHostClient =
    Extensions.ExtHostClient.start(
      ~initData,
      ~onClosed=onExtHostClosed,
      ~onStatusBarSetEntry,
      setup,
    );

  let _bufferMetadataToModelAddedDelta = (bm: Core.Types.BufferMetadata.t) =>
    switch (bm.filePath, bm.fileType) {
    | (Some(fp), Some(_)) =>
      Some(
        Protocol.ModelAddedDelta.create(
          ~uri=Core.Uri.fromPath(fp),
          ~versionId=bm.version,
          ~lines=[""],
          ~modeId="plaintext",
          ~isDirty=true,
          (),
        ),
      )
    /* TODO: filetype detection */
    | (Some(fp), _) =>
      Some(
        Protocol.ModelAddedDelta.create(
          ~uri=Core.Uri.fromPath(fp),
          ~versionId=bm.version,
          ~lines=[""],
          ~modeId="plaintext",
          ~isDirty=true,
          (),
        ),
      )
    | _ => None
    };

  let pumpEffect =
    Isolinear.Effect.create(~name="exthost.pump", () =>
      ExtHostClient.pump(extHostClient)
    );

  let sendBufferEnterEffect = (bm: Core.Types.BufferMetadata.t) =>
    Isolinear.Effect.create(~name="exthost.bufferEnter", () =>
      switch (_bufferMetadataToModelAddedDelta(bm)) {
      | None => ()
      | Some(v) => ExtHostClient.addDocument(v, extHostClient)
      }
    );

  let modelChangedEffect =
      (buffers: Model.Buffers.t, bu: Core.Types.BufferUpdate.t) =>
    Isolinear.Effect.create(~name="exthost.bufferUpdate", () =>
      switch (Model.Buffers.getBuffer(bu.id, buffers)) {
      | None => ()
      | Some(v) =>
        let modelContentChange =
          Protocol.ModelContentChange.ofBufferUpdate(
            bu,
            Protocol.Eol.default,
          );
        let modelChangedEvent =
          Protocol.ModelChangedEvent.create(
            ~changes=[modelContentChange],
            ~eol=Protocol.Eol.default,
            ~versionId=bu.version,
            (),
          );

        let uri = Model.Buffer.getUri(v);

        ExtHostClient.updateDocument(
          uri,
          modelChangedEvent,
          true,
          extHostClient,
        );
      }
    );

  let registerQuitCleanupEffect =
    Isolinear.Effect.createWithDispatch(
      ~name="exthost.registerQuitCleanup", dispatch =>
      dispatch(
        Model.Actions.RegisterQuitCleanup(
          () => ExtHostClient.close(extHostClient),
        ),
      )
    );

  let updater = (state: Model.State.t, action) =>
    switch (action) {
    | Model.Actions.Init => (state, registerQuitCleanupEffect)
    | Model.Actions.BufferUpdate(bu) => (
        state,
        modelChangedEffect(state.buffers, bu),
      )
    | Model.Actions.BufferEnter(bm) => (state, sendBufferEnterEffect(bm))
    | Model.Actions.Tick => (state, pumpEffect)
    | _ => (state, Isolinear.Effect.none)
    };

  (updater, stream);
};
