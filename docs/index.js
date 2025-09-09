import {
  createApp,
  ref,
  computed,
  onMounted,
} from "./vendor/vue.esm-browser.prod.js";

let sections;
const res = await fetch("./sections.json");
sections = await res.json();

export default function createDocsApp() {
  const App = {
    template: `

    <div class="layout">
      <header class="topbar">
        <div class="logo">
          <span class="logo-emoji" aria-hidden="true">🍰</span>
          Napoleon HTTP <span class="badge">Docs</span>
        </div>
        <div class="spacer"></div>
        <button class="btn btn-ghost" @click="toggleTheme" :title="isDark? 'Light' : 'Dark'">
          {{ isDark ? '🌙' : '☀️' }}
        </button>
        <a class="btn" href="/docs/doxygen/">Open /docs/doxygen</a>
        <a class="btn" href="/public/">Open /public</a>
      </header>

      <aside class="sidebar">
        <div class="search">
          <span >🔎</span>
          <input v-model="query" placeholder="Search sections…" />
        </div>
        <ul class="toc">
          <li v-for="section in filteredSections" :key="section.id">
            <a :href="'#'+section.id" :class="{active: currentId===section.id}" @click="currentId=section.id">{{ section.title }}</a>
          </li>
        </ul>
      </aside>

      <main class="content">
        <section v-for="section in filteredSections" :id="section.id" :key="section.id" class="card">
          <h1>{{ section.title }}</h1>
          <p class="muted" v-if="section.subtitle">{{ section.subtitle }}</p>

          <div v-for="block in section.blocks" :key="block.caption || block.html || block.type" style="margin-top:10px">
          
          <!-- HR BLOCK -->
          <template v-if="block.type==='hr'">
              <hr/>
            </template>

          <!-- PARAGRAPH BLOCK -->
            <template v-else-if="block.type==='paragraph'">
              <p v-html="block.html"></p>
            </template>

            <!-- LIST BLOCK -->
            <template v-else-if="block.type==='list'">
              <ul>
                <li v-for="(listItem,index) in block.items" :key="index" v-html="listItem"></li>
              </ul>
            </template>
            
            <!-- CODE BLOCK -->
            <template v-else-if="block.type==='code'">
              <div class="row">
                <span class="pill">{{ block.lang }}</span>
                <span class="muted">{{ block.caption }}</span>
                <div class="spacer"></div>
                <button class="btn" @click="copy(block.code)">Copy</button>
              </div>
              <pre><code>{{ block.code }}</code></pre>
            </template>

            <!-- ECHO GET BLOCK -->
            <template v-else-if="block.type==='echoGet'">
            <div class="row">
              <button class="btn" @click="sendEchoGet">GET /api/echo</button>
              <span class="muted" v-if="echoGetStatus">{{ echoGetStatus }}</span>
            </div>
            <pre v-if="echoGetResponse"><code>Response: {{ echoGetResponse }}</code></pre>
          </template>

            <!-- ECHO POST BLOCK -->
            <template v-else-if="block.type==='echoPost'">
              <div class="row">
                <input v-model="echoPostInput" placeholder='{"msg":"hello"}' class="btn" style="width:320px; text-align:left" />
                <button class="btn" @click="sendEchoPost">POST /api/echo</button>
                <span class="muted" v-if="echoPostStatus">{{ echoPostStatus }}</span>
              </div>
              <pre v-if="echoPostResponse"><code>Response: {{ echoPostResponse }}</code></pre>
            </template>

          </div>
        </section>
      </main>
      <footer class="site-footer">
      <small class="muted">
        © {{currentYear}} Witali Klein— Code licensed under <a href="/public/LICENSE.txt">MIT</a>.
        Images may be under separate licenses (e.g. Pexels).
        <span class="sep">·</span>
        <a href="https://github.com/wklein1/napoleon-http">Source</a>
      </small>
    </footer>
    </div>

    `,

    setup() {
      const systemPrefersDark =
        window.matchMedia &&
        window.matchMedia("(prefers-color-scheme: dark)").matches;
      const savedTheme = localStorage.getItem("theme");
      const initialIsDark = savedTheme
        ? savedTheme === "dark"
        : systemPrefersDark;

      const isDark = ref(initialIsDark);

      const query = ref("");
      const currentId = ref(location.hash ? location.hash.slice(1) : "intro");
      
      const echoGetResponse = ref("");
      const echoGetStatus = ref("");

      const echoPostInput = ref('{"msg":"hello"}');
      const echoPostResponse = ref("");
      const echoPostStatus = ref("");

      const applyThemeAttr = () => {
        document.documentElement.setAttribute("data-theme", isDark.value ? "dark" : "light");
        document.documentElement.style.colorScheme = isDark.value ? "dark" : "light";
      }

      const currentYear = computed(()=>new Date().getFullYear())

      const filteredSections = computed(() => {
        const searchQuery = query.value.toLowerCase().trim();
        if (!searchQuery) return sections;

        return sections.filter(
          //section title
          (section) =>
            section.title.toLowerCase().includes(searchQuery) ||

          //section subtitle
            (section.subtitle &&
              section.subtitle.toLowerCase().includes(searchQuery)) ||

          //section blocks
            (section.blocks || []).some((block) => {

              if (block.type === "p")
                return String(block.html).toLowerCase().includes(searchQuery);

              if (block.type === "code") {
                return (
                  (block.caption || "").toLowerCase().includes(searchQuery) ||
                  (block.code || "").toLowerCase().includes(searchQuery)
                );
              }

              if (block.type === "list") {
                return block.items.some((listItem) =>
                  String(listItem).toLowerCase().includes(searchQuery)
                );
              }

              if (block.type === "echo") return "echo".includes(searchQuery);
              return false;
            })
        );
      });

      const toggleTheme = () => {
        isDark.value = !isDark.value;
        applyThemeAttr();
        localStorage.setItem("theme", isDark.value ? "dark" : "light");
      }

      const copy = async (text) => {
        try {
          await navigator.clipboard.writeText(text);
          showToast("Copied!");
        } catch {
          showToast("Copy failed");
        }
      }

      const showToast = (message) => {
        const toastElement = document.createElement("div");
        toastElement.textContent = message;
        toastElement.className = "toast";
        document.body.appendChild(toastElement);
        setTimeout(() => {
          toastElement.remove();
        }, 1200);
      }

      const sendEchoGet = async () => {
        echoGetStatus.value = "sending…";
        echoGetResponse.value = "";
        try {
          const response = await fetch("/api/echo");
          const responseText = await response.text();
          echoGetStatus.value = `status ${response.status}`;
          echoGetResponse.value = responseText;
        } catch (error) {
          echoGetStatus.value = "request failed";
          echoGetResponse.value = String(error);
        }
      }

      const sendEchoPost = async () => {
        echoPostStatus.value = "sending…";
        echoPostResponse.value = "";
        let requestBody = echoPostInput.value;
        try {
          requestBody = JSON.stringify(JSON.parse(echoPostInput.value));
        } catch { }
        try {
          const response = await fetch("/api/echo", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: requestBody,
          });
          const responseText = await response.text();
          echoPostStatus.value = `status ${response.status}`;
          echoPostResponse.value = responseText;
        } catch (error) {
          echoPostStatus.value = "request failed";
          echoPostResponse.value = String(error);
        }
      }

      onMounted(() => {
        applyThemeAttr();
        window.addEventListener(
          "hashchange",
          () => {
            currentId.value = location.hash.slice(1) || "intro";
          },
          { passive: true }
        );
        
      });

      return {
        sections,
        query,
        currentId,
        isDark,
        echoGetResponse,
        echoGetStatus,
        echoPostInput,
        echoPostResponse,
        echoPostStatus,
        filteredSections,
        currentYear,
        toggleTheme,
        copy,
        sendEchoGet,
        sendEchoPost,

      };
    },
  };

  return createApp(App);
}
