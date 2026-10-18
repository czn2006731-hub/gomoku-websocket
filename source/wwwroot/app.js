(() => {
  "use strict";

  const page = document.body.dataset.page;
  const $ = (selector, root = document) => root.querySelector(selector);
  const socketUrl = (path) => `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}${path}`;

  async function request(path, payload) {
    const response = await fetch(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    let data;
    try {
      data = await response.json();
    } catch (_) {
      throw new Error("服务器返回了无法识别的数据。");
    }
    if (!response.ok || data.result === false) {
      throw new Error(data.reason || "请求未完成，请稍后重试。");
    }
    return data;
  }

  async function getUser() {
    const response = await fetch("/info", { credentials: "same-origin" });
    let data;
    try {
      data = await response.json();
    } catch (_) {
      return null;
    }
    return response.ok && data && data.id !== undefined ? data : null;
  }

  function setStatus(element, text, isError = false) {
    if (!element) return;
    element.textContent = text;
    element.classList.toggle("is-error", isError);
  }

  function initLogin() {
    const notice = $("#notice");
    const tabs = [$("#login-tab"), $("#register-tab")];
    const panels = [$("#login-panel"), $("#register-panel")];

    getUser().then((user) => {
      if (user) location.replace("/index.html");
    });

    function selectTab(index) {
      tabs.forEach((tab, tabIndex) => {
        const selected = tabIndex === index;
        tab.classList.toggle("is-active", selected);
        tab.setAttribute("aria-selected", String(selected));
        panels[tabIndex].hidden = !selected;
      });
      notice.textContent = "";
      notice.classList.remove("is-success");
    }

    tabs.forEach((tab, index) => tab.addEventListener("click", () => selectTab(index)));

    function bindForm(formId, endpoint, successText, redirect) {
      $(formId).addEventListener("submit", async (event) => {
        event.preventDefault();
        const form = event.currentTarget;
        if (!form.reportValidity()) return;
        const submit = $("button[type='submit']", form);
        const formData = new FormData(form);
        submit.disabled = true;
        notice.classList.remove("is-success");
        notice.textContent = "正在处理…";
        try {
          await request(endpoint, {
            username: String(formData.get("username")).trim(),
            password: String(formData.get("password"))
          });
          notice.textContent = successText;
          notice.classList.add("is-success");
          if (redirect) window.setTimeout(() => location.assign("/index.html"), 260);
          else form.reset();
        } catch (error) {
          notice.textContent = error.message;
        } finally {
          submit.disabled = false;
        }
      });
    }

    bindForm("#login-form", "/login", "登录成功，正在进入大厅…", true);
    bindForm("#register-form", "/reg", "注册成功，请使用新账号登录。", false);
  }

  async function initHall() {
    const name = $("#user-name");
    const status = $("#connection-status");
    const match = $("#match-button");
    const cancel = $("#cancel-match-button");
    let hallSocket = null;
    let leavingForRoom = false;
    let matching = false;

    const user = await getUser();
    if (!user) {
      location.replace("/login.html");
      return;
    }
    name.textContent = `${user.username || "未命名用户"} · ${user.score ?? 0} 分`;

    function setMatching(active) {
      matching = active;
      match.disabled = active || !hallSocket || hallSocket.readyState !== WebSocket.OPEN;
      cancel.disabled = !active || !hallSocket || hallSocket.readyState !== WebSocket.OPEN;
    }

    function send(payload) {
      if (!hallSocket || hallSocket.readyState !== WebSocket.OPEN) {
        setStatus(status, "大厅连接尚未就绪，请稍后重试。", true);
        return false;
      }
      hallSocket.send(JSON.stringify(payload));
      return true;
    }

    hallSocket = new WebSocket(socketUrl("/hall"));
    hallSocket.addEventListener("open", () => setStatus(status, "已连接大厅，正在确认身份…"));
    hallSocket.addEventListener("message", (event) => {
      let data;
      try { data = JSON.parse(event.data); } catch (_) { return; }
      if (data.optype === "hall_ready") {
        if (data.result) {
          setStatus(status, "大厅已就绪，可以开始匹配。");
          setMatching(false);
        } else {
          setStatus(status, data.reason || "大厅连接失败。", true);
        }
      } else if (data.optype === "match_start") {
        if (data.result) setStatus(status, "正在为你寻找对手…");
      } else if (data.optype === "match_stop") {
        if (data.result) setStatus(status, "已停止匹配。");
        setMatching(false);
      } else if (data.optype === "match_success" && data.result) {
        leavingForRoom = true;
        setStatus(status, "匹配成功，正在进入房间…");
        match.disabled = true;
        cancel.disabled = true;
        hallSocket.close();
        window.setTimeout(() => location.assign("/room.html"), 220);
      }
    });
    hallSocket.addEventListener("close", () => {
      if (!leavingForRoom) {
        setStatus(status, "大厅连接已断开，请刷新页面重试。", true);
        match.disabled = true;
        cancel.disabled = true;
      }
    });
    hallSocket.addEventListener("error", () => setStatus(status, "无法连接大厅，请确认服务已启动。", true));

    match.addEventListener("click", () => {
      if (send({ optype: "match_start" })) {
        setMatching(true);
        setStatus(status, "正在为你寻找对手…");
      }
    });
    cancel.addEventListener("click", () => {
      if (send({ optype: "match_stop" })) setStatus(status, "正在停止匹配…");
    });
    $("#logout-button").addEventListener("click", async () => {
      leavingForRoom = true;
      try {
        // 让服务端删除 Session 并清除浏览器 Cookie，而不只是跳转页面。
        await fetch("/logout", { method: "POST", credentials: "same-origin" });
      } finally {
        if (hallSocket) hallSocket.close();
        location.assign("/login.html");
      }
    });
  }

  async function initRoom() {
    const board = $("#board");
    const status = $("#game-status");
    const turn = $("#turn-marker");
    const chatLog = $("#chat-log");
    const chatForm = $("#chat-form");
    const chatInput = $("#chat-input");
    const chatSubmit = $("button[type='submit']", chatForm);
    const state = {
      socket: null,
      user: null,
      roomId: null,
      whiteId: null,
      blackId: null,
      whiteName: "",
      blackName: "",
      whiteLevel: 1,
      blackLevel: 1,
      currentTurn: null,
      cells: Array.from({ length: 15 }, () => Array(15).fill(0)),
      ready: false,
      leaving: false,
      attempts: 0,
      retryQueued: false,
      retryTimer: null
    };

    state.user = await getUser();
    if (!state.user) {
      location.replace("/login.html");
      return;
    }

    function renderBoard() {
      board.replaceChildren();
      state.cells.forEach((row, rowIndex) => row.forEach((value, colIndex) => {
        const cell = document.createElement("button");
        cell.type = "button";
        cell.className = "cell";
        cell.dataset.row = String(rowIndex);
        cell.dataset.col = String(colIndex);
        const occupied = value !== 0;
        cell.setAttribute("role", "gridcell");
        cell.setAttribute("aria-disabled", String(occupied || !state.ready));
        cell.setAttribute("aria-label", `第 ${rowIndex + 1} 行，第 ${colIndex + 1} 列${value === 1 ? "，白棋" : value === 2 ? "，黑棋" : "，空位"}`);
        if (occupied) {
          const piece = document.createElement("span");
          piece.className = `piece ${value === 1 ? "piece-white" : "piece-black"}`;
          cell.append(piece);
        }
        board.append(cell);
      }));
    }

    function playerName(uid) {
      if (String(uid) === String(state.whiteId)) return state.whiteName || "未知用户";
      if (String(uid) === String(state.blackId)) return state.blackName || "未知用户";
      return "未知用户";
    }

    function addChat(text, kind = "message", uid = null) {
      const line = document.createElement("p");
      line.className = kind === "system" ? "chat-system" : "chat-message";
      if (uid !== null) {
        const author = document.createElement("b");
        author.textContent = String(uid) === String(state.user.id) ? "我" : playerName(uid);
        line.append(author, document.createTextNode(text));
      } else {
        line.textContent = text;
      }
      chatLog.append(line);
      chatLog.scrollTop = chatLog.scrollHeight;
    }

    function updateTurn() {
      if (!state.ready) return;
      if (state.currentTurn === null) {
        turn.textContent = "等待服务器同步";
        return;
      }
      const isMe = String(state.currentTurn) === String(state.user.id);
      const color = String(state.currentTurn) === String(state.whiteId) ? "白棋" : "黑棋";
      turn.textContent = isMe ? `轮到你落子 · ${color}` : `等待对手落子 · ${color}`;
    }

    function isOpen() {
      return state.socket && state.socket.readyState === WebSocket.OPEN;
    }

    function placeResponse(data) {
      if (!data.result) {
        setStatus(status, data.reason || "本次落子无效。", true);
        return;
      }
      const row = Number(data.row);
      const col = Number(data.col);
      if (Number.isInteger(row) && Number.isInteger(col) && row >= 0 && row < 15 && col >= 0 && col < 15) {
        if (state.cells[row][col] === 0) {
          state.cells[row][col] = String(data.uid) === String(state.whiteId) ? 1 : 2;
          renderBoard();
          state.currentTurn = String(data.uid) === String(state.whiteId) ? state.blackId : state.whiteId;
        }
      }
      if (data.winner && Number(data.winner) !== 0) {
        const won = String(data.winner) === String(state.user.id);
        setStatus(status, won ? "你赢了，本局结束。" : "对手获胜，本局结束。", !won);
        turn.textContent = "本局已结束";
        state.ready = false;
        chatInput.disabled = true;
        chatSubmit.disabled = true;
      } else {
        setStatus(status, data.reason || "落子已同步。");
        updateTurn();
      }
    }

    function retryConnection(reason) {
      if (state.retryQueued) return;
      if (state.leaving || state.attempts >= 6) {
        setStatus(status, reason || "暂时无法进入房间，请返回大厅重新匹配。", true);
        return;
      }
      state.retryQueued = true;
      state.attempts += 1;
      setStatus(status, "房间正在准备，正在重试连接…");
      window.clearTimeout(state.retryTimer);
      state.retryTimer = window.setTimeout(() => {
        state.retryQueued = false;
        connect();
      }, 300);
    }

    function connect() {
      if (state.leaving || state.ready) return;
      if (state.socket) {
        state.socket.onclose = null;
        state.socket.close();
      }
      state.socket = new WebSocket(socketUrl("/room"));
      state.socket.addEventListener("open", () => setStatus(status, "已连接房间，正在同步对局…"));
      state.socket.addEventListener("message", (event) => {
        let data;
        try { data = JSON.parse(event.data); } catch (_) { return; }
        if (data.optype === "room_ready") {
          if (!data.result) {
            state.socket.close();
            retryConnection(data.reason);
            return;
          }
          state.ready = true;
          state.roomId = data.room_id;
          state.whiteId = data.white_id;
          state.blackId = data.black_id;
          state.whiteName = data.white_name || "未知用户";
          state.blackName = data.black_name || "未知用户";
          state.whiteLevel = Number.isInteger(Number(data.white_level)) ? Number(data.white_level) : 1;
          state.blackLevel = Number.isInteger(Number(data.black_level)) ? Number(data.black_level) : 1;
          state.currentTurn = state.whiteId;
          $("#white-name").textContent = state.whiteName;
          $("#black-name").textContent = state.blackName;
          $("#white-level").textContent = `等级 ${state.whiteLevel}`;
          $("#black-level").textContent = `等级 ${state.blackLevel}`;
          setStatus(status, String(state.user.id) === String(state.whiteId) ? "你执白棋，先手落子。" : "你执黑棋，等待白棋先行。 ");
          addChat("房间已准备完成。", "system");
          chatInput.disabled = false;
          chatSubmit.disabled = false;
          renderBoard();
          updateTurn();
        } else if (data.optype === "put_chess") {
          placeResponse(data);
        } else if (data.optype === "chat") {
          if (data.result) addChat(data.message || "", "message", data.uid);
          else setStatus(status, data.reason || "消息未能发送。", true);
        }
      });
      state.socket.addEventListener("error", () => setStatus(status, "房间连接异常。", true));
      state.socket.addEventListener("close", () => {
        if (!state.leaving && !state.ready) retryConnection();
      });
    }

    board.addEventListener("click", (event) => {
      const cell = event.target.closest(".cell");
      if (!cell || !state.ready) return;
      const row = Number(cell.dataset.row);
      const col = Number(cell.dataset.col);
      if (state.cells[row][col] !== 0) return;
      if (String(state.currentTurn) !== String(state.user.id)) {
        setStatus(status, "还没有轮到你落子。", true);
        return;
      }
      if (!isOpen()) {
        setStatus(status, "房间连接已断开，请稍后重试。", true);
        return;
      }
      state.socket.send(JSON.stringify({
        optype: "put_chess",
        room_id: state.roomId,
        uid: state.user.id,
        row,
        col
      }));
      setStatus(status, "正在同步落子…");
    });

    chatForm.addEventListener("submit", (event) => {
      event.preventDefault();
      const message = chatInput.value.trim();
      if (!message || !state.ready || !isOpen()) return;
      state.socket.send(JSON.stringify({
        optype: "chat",
        room_id: state.roomId,
        uid: state.user.id,
        message
      }));
      chatInput.value = "";
    });

    function leaveRoom() {
      state.leaving = true;
      window.clearTimeout(state.retryTimer);
      if (state.socket) state.socket.close();
      location.assign("/index.html");
    }

    $("#leave-room-button").addEventListener("click", leaveRoom);
    window.addEventListener("beforeunload", () => {
      state.leaving = true;
      if (state.socket) state.socket.close();
    });
    renderBoard();
    connect();
  }

  if (page === "login") initLogin();
  if (page === "hall") initHall();
  if (page === "room") initRoom();
})();
