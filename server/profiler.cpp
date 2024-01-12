#include "profiler.hpp"

extern volatile int   exit_flag;
extern t_state_cursor c_state;
extern t_state_frame  f_state;
extern shared_state   state;

const ImVec4 red   = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
const ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 bg    = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);

GLFWwindow * window;

static void show_state_flags() {

  bool     first = false;
  uint32_t st    = state.ctrl.load();
  if (st & S_F_INIT) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "F_INIT");
  }
  if (st & S_C_INIT) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "C_INIT");
  }
  if (st & S_F_PAUSE_LOCAL_UNSYNC) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "F_PAUSE_LOCAL*");
  }
  if (st & S_F_PAUSE_LOCAL_SYNC) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "F_PAUSE_LOCAL");
  }
  if (st & S_F_PAUSE_REMOTE) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "F_PAUSE_REMOTE");
  }
  if (st & S_C_PAUSE_LOCAL_UNSYNC) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "C_PAUSE_LOCAL*");
  }
  if (st & S_C_PAUSE_LOCAL_SYNC) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "C_PAUSE_LOCAL");
  }
  if (st & S_C_PAUSE_REMOTE) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "C_PAUSE_REMOTE");
  }
  if (st & S_EXIT) {
    if (!first)
      ImGui::SameLine();
    else
      first = false;
    ImGui::Text("%s ", "EXIT");
  }
}

static void show_global_state() {
  ImGui::SeparatorText("Global State");
  uint8_t st = state.ctrl.load();

  ImGui::BeginTable("GlobalState", 2);
  ImGui::TableSetupColumn("TableKey", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("TableValue", ImGuiTableColumnFlags_WidthStretch);

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Flags");
  ImGui::TableNextColumn();
  show_state_flags();

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Bootstrap");
  ImGui::TableNextColumn();
  ImGui::Text("%s", state.core_state_str.load());

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Frame Thread");
  ImGui::TableNextColumn();
  ImGui::Text("%s", state.frame_state_str.load());

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Cursor Thread");
  ImGui::TableNextColumn();
  ImGui::Text("%s", state.cursor_state_str.load());

  ImGui::EndTable();
}

static void show_fabric_info() {
  ImGui::SeparatorText("Network Parameters");

  ImGui::BeginTable("StatusTable", 2);
  ImGui::TableSetupColumn("TableKey", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("TableValue", ImGuiTableColumnFlags_WidthStretch);

  fi_info * param =
      (fi_info *) state.resrc->fabric->_get_fi_resource(TCM_RESRC_PARAM);
  if (param) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Transport");
    ImGui::TableNextColumn();
    ImGui::Text("%s", param->fabric_attr->prov_name);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Libfabric");
    ImGui::TableNextColumn();
    ImGui::Text("%d.%d", FI_MAJOR(param->fabric_attr->api_version),
                FI_MINOR(param->fabric_attr->api_version));
  }

  ImGui::EndTable();
}

static void show_frame_buffer_info() {

  ImGui::SeparatorText("Frame buffers");

  ImGui::BeginTable("FrameBufferTable", 2);
  ImGui::TableSetupColumn("Local", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Remote", ImGuiTableColumnFlags_WidthStretch);

  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  // Local side

  ImGui::Text("Local");
  ImGui::TextColored(red, "LGMP disconnected");

  ImGui::TableNextColumn();

  // Remote side

  ImGui::Text("Remote");

  ImGui::Text("Available: %lu of %lu", (uint64_t) f_state.remote_fb.avail(),
              f_state.remote_fb.offsets.size());
  ImGui::Text("Base Address: %p", (void *) (uintptr_t) f_state.remote_fb.base);

  ImGui::Text("Offsets: ");
  if (f_state.remote_fb.offsets.size()) {
    for (size_t i = 0; i < f_state.remote_fb.offsets.size(); ++i) {
      ImGui::SameLine();
      if (f_state.remote_fb.used.at(i)) {
        ImGui::TextColored(red, "%lu", f_state.remote_fb.offsets.at(i));
      } else {
        ImGui::TextColored(green, "%lu", f_state.remote_fb.offsets.at(i));
      }
    }
  }

  ImGui::EndTable();
}

static void show_cursor_buffer_info() {

  ImGui::SeparatorText("Cursor buffers");

  ImGui::BeginTable("CursorBufferTable", 2);
  ImGui::TableSetupColumn("Local", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Remote", ImGuiTableColumnFlags_WidthStretch);

  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  // Local side

  ImGui::Text("Local");
  ImGui::TextColored(red, "LGMP disconnected");

  ImGui::TableNextColumn();

  // Remote side
  ImGui::Text("Remote");

  ImGui::Text("Available: %lu of %lu", (uint64_t) c_state.remote_cb.avail(),
              c_state.remote_cb.offsets.size());
  ImGui::Text("Base Address: %p", (void *) (uintptr_t) c_state.remote_cb.base);

  ImGui::Text("Offsets: ");
  if (c_state.remote_cb.offsets.size()) {
    for (size_t i = 0; i < c_state.remote_cb.offsets.size(); ++i) {
      ImGui::SameLine();
      if (c_state.remote_cb.used.at(i)) {
        ImGui::TextColored(red, "%lu", c_state.remote_cb.offsets.at(i));
      } else {
        ImGui::TextColored(green, "%lu", c_state.remote_cb.offsets.at(i));
      }
    }
  }

  ImGui::EndTable();
}

static void show_lgmp_info() {
  ImGui::SeparatorText("LGMP");
  if (f_state.lgmp.get() && f_state.lgmp->connected()) {
    ImGui::Text("Status: Connected");
  } else {
    ImGui::Text("Status: Disconnected");
  }
}

static void render_out() {
  ImGui::Render();
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  glViewport(0, 0, w, h);
  glClearColor(bg.x * bg.w, bg.y * bg.w, bg.z * bg.w, bg.w);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
}

void * profiler_thread(void * arg) {
  (void) arg;
  // ssize_t ret;

  window = 0;
  if (!glfwInit()) {
    lp_log_error("GLFW initialization error");
    return 0;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  window = glfwCreateWindow(1280, 720, "Test", nullptr, nullptr);

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO & io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 300 es");

  while (!glfwWindowShouldClose(window)) {

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Stats and stuff

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {960.0f, 540.0f});
    ImGui::Begin("Telescope Profiler");
    ImGui::PopStyleVar();

    show_global_state();

    uint8_t a = PROFILER_START, b = PROFILER_START_ACK;
    state.profiler_lock.compare_exchange_strong(a, b,
                                                std::memory_order_acq_rel);

    if (a == PROFILER_START || a == PROFILER_START_ACK) {
      show_fabric_info();
      show_frame_buffer_info();
      show_cursor_buffer_info();
      show_lgmp_info();
    }

    ImGui::End();
    render_out();

    a = PROFILER_STOP, b = PROFILER_STOP_ACK;
    state.profiler_lock.compare_exchange_strong(a, b,
                                                std::memory_order_acq_rel);
    if (a == PROFILER_STOP || a == PROFILER_STOP_ACK)
      continue;

    a = PROFILER_EXIT, b = PROFILER_EXIT_ACK;
    state.profiler_lock.compare_exchange_strong(a, b,
                                                std::memory_order_acq_rel);
    if (a == PROFILER_EXIT || a == PROFILER_EXIT_ACK)
      break;

    if (exit_flag)
      break;
  }

  return 0;
}