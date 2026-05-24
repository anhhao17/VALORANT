<template>
  <div id="app">
    <nav class="navbar">
      <div class="container">
        <h1 class="logo">Jetson BMCweb</h1>
        <ul class="nav-links">
          <li><router-link to="/">Dashboard</router-link></li>
          <li><router-link to="/system">System</router-link></li>
          <li><router-link to="/hwmon">Hardware</router-link></li>
        </ul>
      </div>
    </nav>
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup>
</script>

<style scoped>
.navbar {
  background-color: #1a1a1a;
  color: white;
  padding: 1rem 0;
}

.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.logo {
  margin: 0;
  font-size: 1.5rem;
}

.nav-links {
  list-style: none;
  display: flex;
  gap: 2rem;
  margin: 0;
  padding: 0;
}

.nav-links a {
  color: white;
  text-decoration: none;
  transition: color 0.3s;
}

.nav-links a:hover {
  color: #4CAF50;
}

.main-content {
  max-width: 1200px;
  margin: 2rem auto;
  padding: 0 2rem;
}
</style>
