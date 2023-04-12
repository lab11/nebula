terraform {
  required_providers {
    docker = {
      source = "kreuzwerker/docker"
      version = "~> 3.0.2"
    }
    google = {
      source = "hashicorp/google"
      version = "~> 4.61.0"
    }
    random = {
      source = "hashicorp/random"
      version = "~> 3.5.0"
    }
  }
}

variable "default_region" {
  type = string
  default = "us-central1"
}

variable "default_zone" {
  type = string
  default = "us-central1-a"
}

provider "google" {
  project     = "opportunistic-networks-galaxy"
  region      = var.default_region
}

// --- Store terraform state in GCP storage bucket ---
resource "random_id" "bucket_prefix" {
  byte_length = 8
}

resource "google_storage_bucket" "tfstate" {
  name          = "${random_id.bucket_prefix.hex}-bucket-tfstate"
  location      = var.default_region
  force_destroy = false
  storage_class = "STANDARD"
}
// --- End state storage ---

data "google_iam_policy" "noauth" {
  binding {
    role = "roles/run.invoker"
    members = [
      "allUsers"
    ]
  }
}

resource "google_cloud_run_v2_service" "provider" {
  name     = "provider"
  location = var.default_region
  labels = {
    "galaxy-redis" = "true"
  }

  template {
    containers {
      name = "provider"
      image = "gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest"
      env {
        name = "SERVER_MODE"
        value = "provider"
      }
      env {
        name = "SERVER_PORT"
        value = "443"
      }
      ports {
        container_port = 443
      }
    }
    scaling {
      max_instance_count = 1
    }
    vpc_access {
      connector = google_vpc_access_connector.galaxy_connector.id
      egress = "PRIVATE_RANGES_ONLY"
    }
  }
}

resource "google_cloud_run_service_iam_policy" "provider-noauth" {
  location = google_cloud_run_v2_service.provider.location
  project  = google_cloud_run_v2_service.provider.project
  service  = google_cloud_run_v2_service.provider.name

  policy_data = data.google_iam_policy.noauth.policy_data
}

resource "google_cloud_run_v2_service" "appserver-1" {
  name     = "appserver-1"
  location = var.default_region
  labels = {
    "galaxy-redis" = "true"
  }

  template {
    containers {
      name = "appserver"
      image = "gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest"
      env {
        name = "SERVER_MODE"
        value = "app"
      }
      env {
        name = "SERVER_PORT"
        value = "443"
      }
      env {
        name = "PROVIDER_URL"
        value = "${google_cloud_run_v2_service.provider.uri}"
      }
      ports {
        container_port = 443
      }
    }
    scaling {
      max_instance_count = 1
    }
    vpc_access {
      connector = google_vpc_access_connector.galaxy_connector.id
      egress = "PRIVATE_RANGES_ONLY"
    }
  }
}

resource "google_cloud_run_service_iam_policy" "appserver-1-noauth" {
  location = google_cloud_run_v2_service.appserver-1.location
  project  = google_cloud_run_v2_service.appserver-1.project
  service  = google_cloud_run_v2_service.appserver-1.name

  policy_data = data.google_iam_policy.noauth.policy_data
}

// Redis
resource "google_compute_address" "galaxy-redis" {
  name = "galaxy-redis-ip"
  subnetwork = google_compute_subnetwork.galaxy_net.id
  address_type = "INTERNAL"
}

resource "google_redis_instance" "galaxy-redis" {
  name = "galaxy-redis"
  memory_size_gb = 1

  region = var.default_region
  authorized_network = google_compute_network.galaxy_net.id
  tier = "STANDARD_HA"
  redis_version = "REDIS_4_0"
  reserved_ip_range = google_compute_address.galaxy-redis.address
}

resource "google_compute_firewall" "galaxy-redis-fw" {
  name = "galaxy-redis-fw"
  network = google_compute_network.galaxy_net.id
  allow {
    protocol = "tcp"
    ports = ["6379"]
  }
  source_tags = ["galaxy-redis"]
  target_tags = ["galaxy-redis"]
}

// VPC configuration
resource "google_vpc_access_connector" "galaxy_connector" {
  name   = "galaxy-connector"
  subnet {
    name = google_compute_subnetwork.galaxy_net.name
  }     
  machine_type = "e2-micro"
  region       = var.default_region
  min_instances = 2
  max_instances = 3
}

resource "google_compute_subnetwork" "galaxy_net" {
  name          = "galaxy-subnet"
  ip_cidr_range = "10.0.1.0/24"
  region        = var.default_region
  network       = google_compute_network.galaxy_net.id
}

resource "google_compute_network" "galaxy_net" {
  name                    = "galaxy-net"
  auto_create_subnetworks = false
}