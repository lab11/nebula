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
// --- end ---

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
  }
}

resource "google_cloud_run_service_iam_policy" "appserver-1-noauth" {
  location = google_cloud_run_v2_service.appserver-1.location
  project  = google_cloud_run_v2_service.appserver-1.project
  service  = google_cloud_run_v2_service.appserver-1.name

  policy_data = data.google_iam_policy.noauth.policy_data
}
